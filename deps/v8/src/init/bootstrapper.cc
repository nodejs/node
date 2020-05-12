// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/bootstrapper.h"

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/ieee754.h"
#include "src/builtins/accessors.h"
#include "src/codegen/compiler.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/protectors.h"
#include "src/extensions/cputracemark-extension.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/ignition-statistics-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#ifdef ENABLE_VTUNE_TRACEMARK
#include "src/extensions/vtunedomain-support-extension.h"
#endif  // ENABLE_VTUNE_TRACEMARK
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/numbers/math-random.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments.h"
#include "src/objects/function-kind.h"
#include "src/objects/hash-table-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-display-names.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-string-iterator.h"
#include "src/objects/js-regexp.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-weak-refs.h"
#include "src/objects/property-cell.h"
#include "src/objects/slots-inl.h"
#include "src/objects/templates.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-js.h"

namespace v8 {
namespace internal {

void SourceCodeCache::Initialize(Isolate* isolate, bool create_heap_objects) {
  cache_ = create_heap_objects ? ReadOnlyRoots(isolate).empty_fixed_array()
                               : FixedArray();
}

void SourceCodeCache::Iterate(RootVisitor* v) {
  v->VisitRootPointer(Root::kExtensions, nullptr, FullObjectSlot(&cache_));
}

bool SourceCodeCache::Lookup(Isolate* isolate, Vector<const char> name,
                             Handle<SharedFunctionInfo>* handle) {
  for (int i = 0; i < cache_.length(); i += 2) {
    SeqOneByteString str = SeqOneByteString::cast(cache_.get(i));
    if (str.IsOneByteEqualTo(Vector<const uint8_t>::cast(name))) {
      *handle = Handle<SharedFunctionInfo>(
          SharedFunctionInfo::cast(cache_.get(i + 1)), isolate);
      return true;
    }
  }
  return false;
}

void SourceCodeCache::Add(Isolate* isolate, Vector<const char> name,
                          Handle<SharedFunctionInfo> shared) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  int length = cache_.length();
  Handle<FixedArray> new_array =
      factory->NewFixedArray(length + 2, AllocationType::kOld);
  cache_.CopyTo(0, *new_array, 0, cache_.length());
  cache_ = *new_array;
  Handle<String> str =
      factory
          ->NewStringFromOneByte(Vector<const uint8_t>::cast(name),
                                 AllocationType::kOld)
          .ToHandleChecked();
  DCHECK(!str.is_null());
  cache_.set(length, *str);
  cache_.set(length + 1, *shared);
  Script::cast(shared->script()).set_type(type_);
}

Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION) {}

void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(isolate_, create_heap_objects);
}

static const char* GCFunctionName() {
  bool flag_given =
      FLAG_expose_gc_as != nullptr && strlen(FLAG_expose_gc_as) != 0;
  return flag_given ? FLAG_expose_gc_as : "gc";
}

static bool isValidCpuTraceMarkFunctionName() {
  return FLAG_expose_cputracemark_as != nullptr &&
         strlen(FLAG_expose_cputracemark_as) != 0;
}

void Bootstrapper::InitializeOncePerProcess() {
  v8::RegisterExtension(std::make_unique<GCExtension>(GCFunctionName()));
  v8::RegisterExtension(std::make_unique<ExternalizeStringExtension>());
  v8::RegisterExtension(std::make_unique<StatisticsExtension>());
  v8::RegisterExtension(std::make_unique<TriggerFailureExtension>());
  v8::RegisterExtension(std::make_unique<IgnitionStatisticsExtension>());
  if (isValidCpuTraceMarkFunctionName()) {
    v8::RegisterExtension(
        std::make_unique<CpuTraceMarkExtension>(FLAG_expose_cputracemark_as));
  }
#ifdef ENABLE_VTUNE_TRACEMARK
  v8::RegisterExtension(
      std::make_unique<VTuneDomainSupportExtension>("vtunedomainmark"));
#endif  // ENABLE_VTUNE_TRACEMARK
}

void Bootstrapper::TearDown() {
  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}

class Genesis {
 public:
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template,
          size_t context_snapshot_index,
          v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
          v8::MicrotaskQueue* microtask_queue);
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template);
  ~Genesis() = default;

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Builtins* builtins() const { return isolate_->builtins(); }
  Heap* heap() const { return isolate_->heap(); }

  Handle<Context> result() { return result_; }

  Handle<JSGlobalProxy> global_proxy() { return global_proxy_; }

 private:
  Handle<NativeContext> native_context() { return native_context_; }

  // Creates some basic objects. Used for creating a context from scratch.
  void CreateRoots();
  // Creates the empty function.  Used for creating a context from scratch.
  Handle<JSFunction> CreateEmptyFunction();
  // Returns the %ThrowTypeError% intrinsic function.
  // See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> GetThrowTypeErrorIntrinsic();

  void CreateSloppyModeFunctionMaps(Handle<JSFunction> empty);
  void CreateStrictModeFunctionMaps(Handle<JSFunction> empty);
  void CreateObjectFunction(Handle<JSFunction> empty);
  void CreateIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncFunctionMaps(Handle<JSFunction> empty);
  void CreateJSProxyMaps();

  // Make the "arguments" and "caller" properties throw a TypeError on access.
  void AddRestrictedFunctionProperties(Handle<JSFunction> empty);

  // Creates the global objects using the global proxy and the template passed
  // in through the API.  We call this regardless of whether we are building a
  // context from scratch or using a deserialized one from the partial snapshot
  // but in the latter case we don't use the objects it produces directly, as
  // we have to use the deserialized ones that are linked together with the
  // rest of the context snapshot. At the end we link the global proxy and the
  // context to each other.
  Handle<JSGlobalObject> CreateNewGlobals(
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the global that has been created by the templates
  // passed through the API.  The global from the snapshot is detached from the
  // other objects in the snapshot.
  void HookUpGlobalObject(Handle<JSGlobalObject> global_object);
  // Hooks the given global proxy into the context in the case we do not
  // replace the global object from the deserialized native context.
  void HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy);
  // The native context has a ScriptContextTable that store declarative bindings
  // made in script scopes.  Add a "this" binding to that table pointing to the
  // global proxy.
  void InstallGlobalThisBinding();
  // New context initialization.  Used for creating a context from scratch.
  void InitializeGlobal(Handle<JSGlobalObject> global_object,
                        Handle<JSFunction> empty_function);
  void InitializeExperimentalGlobal();
  void InitializeIteratorFunctions();
  void InitializeCallSiteBuiltins();

#define DECLARE_FEATURE_INITIALIZATION(id, descr) void InitializeGlobal_##id();

  HARMONY_INPROGRESS(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_STAGED(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_SHIPPING(DECLARE_FEATURE_INITIALIZATION)
#undef DECLARE_FEATURE_INITIALIZATION

  enum ArrayBufferKind {
    ARRAY_BUFFER,
    SHARED_ARRAY_BUFFER,
  };
  Handle<JSFunction> CreateArrayBuffer(Handle<String> name,
                                       ArrayBufferKind array_buffer_kind);

  bool InstallABunchOfRandomThings();
  bool InstallExtrasBindings();

  Handle<JSFunction> InstallTypedArray(const char* name,
                                       ElementsKind elements_kind);
  void InitializeNormalizedMapCaches();

  enum ExtensionTraversalState { UNVISITED, VISITED, INSTALLED };

  class ExtensionStates {
   public:
    ExtensionStates();
    ExtensionTraversalState get_state(RegisteredExtension* extension);
    void set_state(RegisteredExtension* extension,
                   ExtensionTraversalState state);

   private:
    base::HashMap map_;
    DISALLOW_COPY_AND_ASSIGN(ExtensionStates);
  };

  // Used both for deserialized and from-scratch contexts to add the extensions
  // provided.
  static bool InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states);
  static bool InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate, const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static bool InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context);
  bool ConfigureApiObject(Handle<JSObject> object,
                          Handle<ObjectTemplateInfo> object_template);
  bool ConfigureGlobalObjects(
      v8::Local<v8::ObjectTemplate> global_proxy_template);

  // Migrates all properties from the 'from' object to the 'to'
  // object and overrides the prototype in 'to' with the one from
  // 'from'.
  void TransferObject(Handle<JSObject> from, Handle<JSObject> to);
  void TransferNamedProperties(Handle<JSObject> from, Handle<JSObject> to);
  void TransferIndexedProperties(Handle<JSObject> from, Handle<JSObject> to);

  Handle<Map> CreateInitialMapForArraySubclass(int size,
                                               int inobject_properties);

  static bool CompileExtension(Isolate* isolate, v8::Extension* extension);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<NativeContext> native_context_;
  Handle<JSGlobalProxy> global_proxy_;

  // Temporary function maps needed only during bootstrapping.
  Handle<Map> strict_function_with_home_object_map_;
  Handle<Map> strict_function_with_name_and_home_object_map_;

  // %ThrowTypeError%. See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> restricted_properties_thrower_;

  BootstrapperActive active_;
  friend class Bootstrapper;
};

void Bootstrapper::Iterate(RootVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}

Handle<Context> Bootstrapper::CreateEnvironment(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  HandleScope scope(isolate_);
  Handle<Context> env;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,
                    context_snapshot_index, embedder_fields_deserializer,
                    microtask_queue);
    env = genesis.result();
    if (env.is_null() || !InstallExtensions(env, extensions)) {
      return Handle<Context>();
    }
  }
  LogAllMaps();
  isolate_->heap()->NotifyBootstrapComplete();
  return scope.CloseAndEscape(env);
}

Handle<JSGlobalProxy> Bootstrapper::NewRemoteContext(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  HandleScope scope(isolate_);
  Handle<JSGlobalProxy> global_proxy;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template);
    global_proxy = genesis.global_proxy();
    if (global_proxy.is_null()) return Handle<JSGlobalProxy>();
  }
  LogAllMaps();
  return scope.CloseAndEscape(global_proxy);
}

void Bootstrapper::LogAllMaps() {
  if (!FLAG_trace_maps || isolate_->initialized_from_snapshot()) return;
  // Log all created Map objects that are on the heap. For snapshots the Map
  // logging happens during deserialization in order to avoid printing Maps
  // multiple times during partial deserialization.
  LOG(isolate_, LogAllMaps());
}

void Bootstrapper::DetachGlobal(Handle<Context> env) {
  isolate_->counters()->errors_thrown_per_context()->AddSample(
      env->native_context().GetErrorsThrown());

  ReadOnlyRoots roots(isolate_);
  Handle<JSGlobalProxy> global_proxy(env->global_proxy(), isolate_);
  global_proxy->set_native_context(roots.null_value());
  JSObject::ForceSetPrototype(global_proxy, isolate_->factory()->null_value());
  global_proxy->map().SetConstructor(roots.null_value());
  if (FLAG_track_detached_contexts) {
    isolate_->AddDetachedContext(env);
  }

  env->native_context().set_microtask_queue(nullptr);
}

namespace {

V8_NOINLINE Handle<SharedFunctionInfo> SimpleCreateSharedFunctionInfo(
    Isolate* isolate, Builtins::Name builtin_id, Handle<String> name, int len,
    FunctionKind kind = FunctionKind::kNormalFunction) {
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(name, builtin_id,
                                                          kind);
  shared->set_internal_formal_parameter_count(len);
  shared->set_length(len);
  return shared;
}

V8_NOINLINE Handle<SharedFunctionInfo> SimpleCreateBuiltinSharedFunctionInfo(
    Isolate* isolate, Builtins::Name builtin_id, Handle<String> name, int len) {
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(name, builtin_id,
                                                          kNormalFunction);
  shared->set_internal_formal_parameter_count(len);
  shared->set_length(len);
  return shared;
}

V8_NOINLINE Handle<JSFunction> CreateFunction(
    Isolate* isolate, Handle<String> name, InstanceType type, int instance_size,
    int inobject_properties, Handle<HeapObject> prototype,
    Builtins::Name builtin_id) {
  DCHECK(Builtins::HasJSLinkage(builtin_id));

  Handle<JSFunction> result;

  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
      name, prototype, type, instance_size, inobject_properties, builtin_id,
      IMMUTABLE);

  result = isolate->factory()->NewFunction(args);
  // Make the JSFunction's prototype object fast.
  JSObject::MakePrototypesFast(handle(result->prototype(), isolate),
                               kStartAtReceiver, isolate);

  // Make the resulting JSFunction object fast.
  JSObject::MakePrototypesFast(result, kStartAtReceiver, isolate);
  result->shared().set_native(true);
  return result;
}

V8_NOINLINE Handle<JSFunction> CreateFunction(
    Isolate* isolate, const char* name, InstanceType type, int instance_size,
    int inobject_properties, Handle<HeapObject> prototype,
    Builtins::Name builtin_id) {
  return CreateFunction(
      isolate, isolate->factory()->InternalizeUtf8String(name), type,
      instance_size, inobject_properties, prototype, builtin_id);
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, Handle<String> name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtins::Name call) {
  DCHECK(Builtins::HasJSLinkage(call));
  Handle<JSFunction> function = CreateFunction(
      isolate, name, type, instance_size, inobject_properties, prototype, call);
  JSObject::AddProperty(isolate, target, name, function, DONT_ENUM);
  return function;
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, const char* name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtins::Name call) {
  return InstallFunction(isolate, target,
                         isolate->factory()->InternalizeUtf8String(name), type,
                         instance_size, inobject_properties, prototype, call);
}

V8_NOINLINE Handle<JSFunction> SimpleCreateFunction(Isolate* isolate,
                                                    Handle<String> name,
                                                    Builtins::Name call,
                                                    int len, bool adapt) {
  DCHECK(Builtins::HasJSLinkage(call));
  name = String::Flatten(isolate, name, AllocationType::kOld);
  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithoutPrototype(
      name, call, LanguageMode::kStrict);
  Handle<JSFunction> fun = isolate->factory()->NewFunction(args);
  // Make the resulting JSFunction object fast.
  JSObject::MakePrototypesFast(fun, kStartAtReceiver, isolate);
  fun->shared().set_native(true);

  if (adapt) {
    fun->shared().set_internal_formal_parameter_count(len);
  } else {
    fun->shared().DontAdaptArguments();
  }
  fun->shared().set_length(len);
  return fun;
}

V8_NOINLINE Handle<JSFunction> InstallFunctionWithBuiltinId(
    Isolate* isolate, Handle<JSObject> base, const char* name,
    Builtins::Name call, int len, bool adapt) {
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_name, call, len, adapt);
  JSObject::AddProperty(isolate, base, internalized_name, fun, DONT_ENUM);
  return fun;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, const char* name,
    Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM) {
  // Although function name does not have to be internalized the property name
  // will be internalized during property addition anyway, so do it here now.
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_name, call, len, adapt);
  JSObject::AddProperty(isolate, base, internalized_name, fun, attrs);
  return fun;
}

V8_NOINLINE Handle<JSFunction> InstallFunctionAtSymbol(
    Isolate* isolate, Handle<JSObject> base, Handle<Symbol> symbol,
    const char* symbol_string, Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM) {
  Handle<String> internalized_symbol =
      isolate->factory()->InternalizeUtf8String(symbol_string);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_symbol, call, len, adapt);
  JSObject::AddProperty(isolate, base, symbol, fun, attrs);
  return fun;
}

V8_NOINLINE void SimpleInstallGetterSetter(Isolate* isolate,
                                           Handle<JSObject> base,
                                           Handle<String> name,
                                           Builtins::Name call_getter,
                                           Builtins::Name call_setter) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call_getter, 0, true);

  Handle<String> setter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
          .ToHandleChecked();
  Handle<JSFunction> setter =
      SimpleCreateFunction(isolate, setter_name, call_setter, 1, true);

  JSObject::DefineAccessor(base, name, getter, setter, DONT_ENUM).Check();
}

void SimpleInstallGetterSetter(Isolate* isolate, Handle<JSObject> base,
                               const char* name, Builtins::Name call_getter,
                               Builtins::Name call_setter) {
  SimpleInstallGetterSetter(isolate, base,
                            isolate->factory()->InternalizeUtf8String(name),
                            call_getter, call_setter);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(
    Isolate* isolate, Handle<JSObject> base, Handle<Name> name,
    Handle<Name> property_name, Builtins::Name call, bool adapt) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call, 0, adapt);

  Handle<Object> setter = isolate->factory()->undefined_value();

  JSObject::DefineAccessor(base, property_name, getter, setter, DONT_ENUM)
      .Check();

  return getter;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(Isolate* isolate,
                                                   Handle<JSObject> base,
                                                   Handle<Name> name,
                                                   Builtins::Name call,
                                                   bool adapt) {
  return SimpleInstallGetter(isolate, base, name, name, call, adapt);
}

V8_NOINLINE void InstallConstant(Isolate* isolate, Handle<JSObject> holder,
                                 const char* name, Handle<Object> value) {
  JSObject::AddProperty(
      isolate, holder, isolate->factory()->InternalizeUtf8String(name), value,
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
}

V8_NOINLINE void InstallTrueValuedProperty(Isolate* isolate,
                                           Handle<JSObject> holder,
                                           const char* name) {
  JSObject::AddProperty(isolate, holder,
                        isolate->factory()->InternalizeUtf8String(name),
                        isolate->factory()->true_value(), NONE);
}

V8_NOINLINE void InstallSpeciesGetter(Isolate* isolate,
                                      Handle<JSFunction> constructor) {
  Factory* factory = isolate->factory();
  // TODO(adamk): We should be able to share a SharedFunctionInfo
  // between all these JSFunctins.
  SimpleInstallGetter(isolate, constructor, factory->symbol_species_string(),
                      factory->species_symbol(), Builtins::kReturnReceiver,
                      true);
}

V8_NOINLINE void InstallToStringTag(Isolate* isolate, Handle<JSObject> holder,
                                    Handle<String> value) {
  JSObject::AddProperty(isolate, holder,
                        isolate->factory()->to_string_tag_symbol(), value,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
}

void InstallToStringTag(Isolate* isolate, Handle<JSObject> holder,
                        const char* value) {
  InstallToStringTag(isolate, holder,
                     isolate->factory()->InternalizeUtf8String(value));
}

}  // namespace

Handle<JSFunction> Genesis::CreateEmptyFunction() {
  // Allocate the function map first and then patch the prototype later.
  Handle<Map> empty_function_map = factory()->CreateSloppyFunctionMap(
      FUNCTION_WITHOUT_PROTOTYPE, MaybeHandle<JSFunction>());
  empty_function_map->set_is_prototype_map(true);
  DCHECK(!empty_function_map->is_dictionary_map());

  // Allocate the empty function as the prototype for function according to
  // ES#sec-properties-of-the-function-prototype-object
  NewFunctionArgs args = NewFunctionArgs::ForBuiltin(
      factory()->empty_string(), empty_function_map, Builtins::kEmptyFunction);
  Handle<JSFunction> empty_function = factory()->NewFunction(args);
  native_context()->set_empty_function(*empty_function);

  // --- E m p t y ---
  Handle<String> source = factory()->NewStringFromStaticChars("() {}");
  Handle<Script> script = factory()->NewScript(source);
  script->set_type(Script::TYPE_NATIVE);
  Handle<WeakFixedArray> infos = factory()->NewWeakFixedArray(2);
  script->set_shared_function_infos(*infos);
  empty_function->shared().set_raw_scope_info(
      ReadOnlyRoots(isolate()).empty_function_scope_info());
  empty_function->shared().DontAdaptArguments();
  empty_function->shared().SetScript(ReadOnlyRoots(isolate()), *script, 1);

  return empty_function;
}

void Genesis::CreateSloppyModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for sloppy functions without prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_sloppy_function_without_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with readonly prototype.
  //
  map =
      factory->CreateSloppyFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_readonly_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with writable prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_sloppy_function_map(*map);

  map = factory->CreateSloppyFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_name_map(*map);
}

Handle<JSFunction> Genesis::GetThrowTypeErrorIntrinsic() {
  if (!restricted_properties_thrower_.is_null()) {
    return restricted_properties_thrower_;
  }
  Handle<String> name = factory()->empty_string();
  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithoutPrototype(
      name, Builtins::kStrictPoisonPillThrower, i::LanguageMode::kStrict);
  Handle<JSFunction> function = factory()->NewFunction(args);
  function->shared().DontAdaptArguments();

  // %ThrowTypeError% must not have a name property.
  if (JSReceiver::DeleteProperty(function, factory()->name_string())
          .IsNothing()) {
    DCHECK(false);
  }

  // length needs to be non configurable.
  Handle<Object> value(Smi::FromInt(function->length()), isolate());
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->length_string(), value,
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  if (JSObject::PreventExtensions(function, kThrowOnError).IsNothing()) {
    DCHECK(false);
  }

  JSObject::MigrateSlowToFast(function, 0, "Bootstrapping");

  restricted_properties_thrower_ = function;
  return function;
}

void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for strict functions without prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_strict_function_without_prototype_map(*map);

  map = factory->CreateStrictFunctionMap(METHOD_WITH_NAME, empty);
  native_context()->set_method_with_name_map(*map);

  map = factory->CreateStrictFunctionMap(METHOD_WITH_HOME_OBJECT, empty);
  native_context()->set_method_with_home_object_map(*map);

  map =
      factory->CreateStrictFunctionMap(METHOD_WITH_NAME_AND_HOME_OBJECT, empty);
  native_context()->set_method_with_name_and_home_object_map(*map);

  //
  // Allocate maps for strict functions with writable prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_strict_function_map(*map);

  map = factory->CreateStrictFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_strict_function_with_name_map(*map);

  strict_function_with_home_object_map_ = factory->CreateStrictFunctionMap(
      FUNCTION_WITH_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE, empty);
  strict_function_with_name_and_home_object_map_ =
      factory->CreateStrictFunctionMap(
          FUNCTION_WITH_NAME_AND_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE, empty);

  //
  // Allocate maps for strict functions with readonly prototype.
  //
  map =
      factory->CreateStrictFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_function_with_readonly_prototype_map(*map);

  //
  // Allocate map for class functions.
  //
  map = factory->CreateClassFunctionMap(empty);
  native_context()->set_class_function_map(*map);

  // Now that the strict mode function map is available, set up the
  // restricted "arguments" and "caller" getters.
  AddRestrictedFunctionProperties(empty);
}

void Genesis::CreateObjectFunction(Handle<JSFunction> empty_function) {
  Factory* factory = isolate_->factory();

  // --- O b j e c t ---
  int inobject_properties = JSObject::kInitialGlobalObjectUnusedPropertiesCount;
  int instance_size = JSObject::kHeaderSize + kTaggedSize * inobject_properties;

  Handle<JSFunction> object_fun = CreateFunction(
      isolate_, factory->Object_string(), JS_OBJECT_TYPE, instance_size,
      inobject_properties, factory->null_value(), Builtins::kObjectConstructor);
  object_fun->shared().set_length(1);
  object_fun->shared().DontAdaptArguments();
  native_context()->set_object_function(*object_fun);

  {
    // Finish setting up Object function's initial map.
    Map initial_map = object_fun->initial_map();
    initial_map.set_elements_kind(HOLEY_ELEMENTS);
  }

  // Allocate a new prototype for the object function.
  Handle<JSObject> object_function_prototype =
      factory->NewFunctionPrototype(object_fun);

  Handle<Map> map =
      Map::Copy(isolate(), handle(object_function_prototype->map(), isolate()),
                "EmptyObjectPrototype");
  map->set_is_prototype_map(true);
  // Ban re-setting Object.prototype.__proto__ to prevent Proxy security bug
  map->set_is_immutable_proto(true);
  object_function_prototype->set_map(*map);

  // Complete setting up empty function.
  {
    Handle<Map> empty_function_map(empty_function->map(), isolate_);
    Map::SetPrototype(isolate(), empty_function_map, object_function_prototype);
  }

  native_context()->set_initial_object_prototype(*object_function_prototype);
  JSFunction::SetPrototype(object_fun, object_function_prototype);

  {
    // Set up slow map for Object.create(null) instances without in-object
    // properties.
    Handle<Map> map(object_fun->initial_map(), isolate_);
    map = Map::CopyInitialMapNormalized(isolate(), map);
    Map::SetPrototype(isolate(), map, factory->null_value());
    native_context()->set_slow_object_with_null_prototype_map(*map);

    // Set up slow map for literals with too many properties.
    map = Map::Copy(isolate(), map, "slow_object_with_object_prototype_map");
    Map::SetPrototype(isolate(), map, object_function_prototype);
    native_context()->set_slow_object_with_object_prototype_map(*map);
  }
}

namespace {

Handle<Map> CreateNonConstructorMap(Isolate* isolate, Handle<Map> source_map,
                                    Handle<JSObject> prototype,
                                    const char* reason) {
  Handle<Map> map = Map::Copy(isolate, source_map, reason);
  // Ensure the resulting map has prototype slot (it is necessary for storing
  // inital map even when the prototype property is not required).
  if (!map->has_prototype_slot()) {
    // Re-set the unused property fields after changing the instance size.
    // TODO(ulan): Do not change instance size after map creation.
    int unused_property_fields = map->UnusedPropertyFields();
    map->set_instance_size(map->instance_size() + kTaggedSize);
    // The prototype slot shifts the in-object properties area by one slot.
    map->SetInObjectPropertiesStartInWords(
        map->GetInObjectPropertiesStartInWords() + 1);
    map->set_has_prototype_slot(true);
    map->SetInObjectUnusedPropertyFields(unused_property_fields);
  }
  map->set_is_constructor(false);
  Map::SetPrototype(isolate, map, prototype);
  return map;
}

}  // namespace

void Genesis::CreateIteratorMaps(Handle<JSFunction> empty) {
  // Create iterator-related meta-objects.
  Handle<JSObject> iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  InstallFunctionAtSymbol(isolate(), iterator_prototype,
                          factory()->iterator_symbol(), "[Symbol.iterator]",
                          Builtins::kReturnReceiver, 0, true);
  native_context()->set_initial_iterator_prototype(*iterator_prototype);

  Handle<JSObject> generator_object_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  native_context()->set_initial_generator_prototype(
      *generator_object_prototype);
  JSObject::ForceSetPrototype(generator_object_prototype, iterator_prototype);
  Handle<JSObject> generator_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  JSObject::ForceSetPrototype(generator_function_prototype, empty);

  InstallToStringTag(isolate(), generator_function_prototype,
                     "GeneratorFunction");
  JSObject::AddProperty(isolate(), generator_function_prototype,
                        factory()->prototype_string(),
                        generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::AddProperty(isolate(), generator_object_prototype,
                        factory()->constructor_string(),
                        generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  InstallToStringTag(isolate(), generator_object_prototype, "Generator");
  SimpleInstallFunction(isolate(), generator_object_prototype, "next",
                        Builtins::kGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "return",
                        Builtins::kGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "throw",
                        Builtins::kGeneratorPrototypeThrow, 1, false);

  // Internal version of generator_prototype_next, flagged as non-native such
  // that it doesn't show up in Error traces.
  Handle<JSFunction> generator_next_internal =
      SimpleCreateFunction(isolate(), factory()->next_string(),
                           Builtins::kGeneratorPrototypeNext, 1, false);
  generator_next_internal->shared().set_native(false);
  native_context()->set_generator_next_internal(*generator_next_internal);

  // Internal version of async module functions, flagged as non-native such
  // that they don't show up in Error traces.
  {
    Handle<JSFunction> async_module_evaluate_internal =
        SimpleCreateFunction(isolate(), factory()->next_string(),
                             Builtins::kAsyncModuleEvaluate, 1, false);
    async_module_evaluate_internal->shared().set_native(false);
    native_context()->set_async_module_evaluate_internal(
        *async_module_evaluate_internal);

    Handle<JSFunction> call_async_module_fulfilled =
        SimpleCreateFunction(isolate(), factory()->empty_string(),
                             Builtins::kCallAsyncModuleFulfilled, 1, false);
    native_context()->set_call_async_module_fulfilled(
        *call_async_module_fulfilled);

    Handle<JSFunction> call_async_module_rejected =
        SimpleCreateFunction(isolate(), factory()->empty_string(),
                             Builtins::kCallAsyncModuleRejected, 1, false);
    native_context()->set_call_async_module_rejected(
        *call_async_module_rejected);
  }

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                generator_function_prototype,
                                "GeneratorFunction");
  native_context()->set_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      generator_function_prototype, "GeneratorFunction with name");
  native_context()->set_generator_function_with_name_map(*map);

  map = CreateNonConstructorMap(
      isolate(), strict_function_with_home_object_map_,
      generator_function_prototype, "GeneratorFunction with home object");
  native_context()->set_generator_function_with_home_object_map(*map);

  map = CreateNonConstructorMap(isolate(),
                                strict_function_with_name_and_home_object_map_,
                                generator_function_prototype,
                                "GeneratorFunction with name and home object");
  native_context()->set_generator_function_with_name_and_home_object_map(*map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), generator_object_prototype_map,
                    generator_object_prototype);
  native_context()->set_generator_object_prototype_map(
      *generator_object_prototype_map);
}

void Genesis::CreateAsyncIteratorMaps(Handle<JSFunction> empty) {
  // %AsyncIteratorPrototype%
  // proposal-async-iteration/#sec-asynciteratorprototype
  Handle<JSObject> async_iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  InstallFunctionAtSymbol(
      isolate(), async_iterator_prototype, factory()->async_iterator_symbol(),
      "[Symbol.asyncIterator]", Builtins::kReturnReceiver, 0, true);

  // %AsyncFromSyncIteratorPrototype%
  // proposal-async-iteration/#sec-%asyncfromsynciteratorprototype%-object
  Handle<JSObject> async_from_sync_iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "next",
                        Builtins::kAsyncFromSyncIteratorPrototypeNext, 1, true);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "return",
                        Builtins::kAsyncFromSyncIteratorPrototypeReturn, 1,
                        true);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "throw",
                        Builtins::kAsyncFromSyncIteratorPrototypeThrow, 1,
                        true);

  InstallToStringTag(isolate(), async_from_sync_iterator_prototype,
                     "Async-from-Sync Iterator");

  JSObject::ForceSetPrototype(async_from_sync_iterator_prototype,
                              async_iterator_prototype);

  Handle<Map> async_from_sync_iterator_map = factory()->NewMap(
      JS_ASYNC_FROM_SYNC_ITERATOR_TYPE, JSAsyncFromSyncIterator::kHeaderSize);
  Map::SetPrototype(isolate(), async_from_sync_iterator_map,
                    async_from_sync_iterator_prototype);
  native_context()->set_async_from_sync_iterator_map(
      *async_from_sync_iterator_map);

  // Async Generators
  Handle<JSObject> async_generator_object_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  Handle<JSObject> async_generator_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  // %AsyncGenerator% / %AsyncGeneratorFunction%.prototype
  JSObject::ForceSetPrototype(async_generator_function_prototype, empty);

  // The value of AsyncGeneratorFunction.prototype.prototype is the
  //     %AsyncGeneratorPrototype% intrinsic object.
  // This property has the attributes
  //     { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
  JSObject::AddProperty(isolate(), async_generator_function_prototype,
                        factory()->prototype_string(),
                        async_generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), async_generator_object_prototype,
                        factory()->constructor_string(),
                        async_generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  InstallToStringTag(isolate(), async_generator_function_prototype,
                     "AsyncGeneratorFunction");

  // %AsyncGeneratorPrototype%
  JSObject::ForceSetPrototype(async_generator_object_prototype,
                              async_iterator_prototype);
  native_context()->set_initial_async_generator_prototype(
      *async_generator_object_prototype);

  InstallToStringTag(isolate(), async_generator_object_prototype,
                     "AsyncGenerator");
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "next",
                        Builtins::kAsyncGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "return",
                        Builtins::kAsyncGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "throw",
                        Builtins::kAsyncGeneratorPrototypeThrow, 1, false);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Async Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                async_generator_function_prototype,
                                "AsyncGeneratorFunction");
  native_context()->set_async_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      async_generator_function_prototype, "AsyncGeneratorFunction with name");
  native_context()->set_async_generator_function_with_name_map(*map);

  map =
      CreateNonConstructorMap(isolate(), strict_function_with_home_object_map_,
                              async_generator_function_prototype,
                              "AsyncGeneratorFunction with home object");
  native_context()->set_async_generator_function_with_home_object_map(*map);

  map = CreateNonConstructorMap(
      isolate(), strict_function_with_name_and_home_object_map_,
      async_generator_function_prototype,
      "AsyncGeneratorFunction with name and home object");
  native_context()->set_async_generator_function_with_name_and_home_object_map(
      *map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> async_generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), async_generator_object_prototype_map,
                    async_generator_object_prototype);
  native_context()->set_async_generator_object_prototype_map(
      *async_generator_object_prototype_map);
}

void Genesis::CreateAsyncFunctionMaps(Handle<JSFunction> empty) {
  // %AsyncFunctionPrototype% intrinsic
  Handle<JSObject> async_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  JSObject::ForceSetPrototype(async_function_prototype, empty);

  InstallToStringTag(isolate(), async_function_prototype, "AsyncFunction");

  Handle<Map> map =
      Map::Copy(isolate(), isolate()->strict_function_without_prototype_map(),
                "AsyncFunction");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_map(*map);

  map = Map::Copy(isolate(), isolate()->method_with_name_map(),
                  "AsyncFunction with name");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_with_name_map(*map);

  map = Map::Copy(isolate(), isolate()->method_with_home_object_map(),
                  "AsyncFunction with home object");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_with_home_object_map(*map);

  map = Map::Copy(isolate(), isolate()->method_with_name_and_home_object_map(),
                  "AsyncFunction with name and home object");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_with_name_and_home_object_map(*map);
}

void Genesis::CreateJSProxyMaps() {
  // Allocate maps for all Proxy types.
  // Next to the default proxy, we need maps indicating callable and
  // constructable proxies.
  Handle<Map> proxy_map = factory()->NewMap(JS_PROXY_TYPE, JSProxy::kSize,
                                            TERMINAL_FAST_ELEMENTS_KIND);
  proxy_map->set_is_dictionary_map(true);
  proxy_map->set_may_have_interesting_symbols(true);
  native_context()->set_proxy_map(*proxy_map);

  Handle<Map> proxy_callable_map =
      Map::Copy(isolate_, proxy_map, "callable Proxy");
  proxy_callable_map->set_is_callable(true);
  native_context()->set_proxy_callable_map(*proxy_callable_map);
  proxy_callable_map->SetConstructor(native_context()->function_function());

  Handle<Map> proxy_constructor_map =
      Map::Copy(isolate_, proxy_callable_map, "constructor Proxy");
  proxy_constructor_map->set_is_constructor(true);
  native_context()->set_proxy_constructor_map(*proxy_constructor_map);

  {
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSProxyRevocableResult::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 2);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // proxy
      Descriptor d = Descriptor::DataField(isolate(), factory()->proxy_string(),
                                           JSProxyRevocableResult::kProxyIndex,
                                           NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // revoke
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->revoke_string(),
          JSProxyRevocableResult::kRevokeIndex, NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_proxy_revocable_result_map(*map);
  }
}

namespace {
void ReplaceAccessors(Isolate* isolate, Handle<Map> map, Handle<String> name,
                      PropertyAttributes attributes,
                      Handle<AccessorPair> accessor_pair) {
  DescriptorArray descriptors = map->instance_descriptors();
  InternalIndex entry = descriptors.SearchWithCache(isolate, *name, *map);
  Descriptor d = Descriptor::AccessorConstant(name, accessor_pair, attributes);
  descriptors.Replace(entry, &d);
}
}  // namespace

void Genesis::AddRestrictedFunctionProperties(Handle<JSFunction> empty) {
  PropertyAttributes rw_attribs = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> thrower = GetThrowTypeErrorIntrinsic();
  Handle<AccessorPair> accessors = factory()->NewAccessorPair();
  accessors->set_getter(*thrower);
  accessors->set_setter(*thrower);

  Handle<Map> map(empty->map(), isolate());
  ReplaceAccessors(isolate(), map, factory()->arguments_string(), rw_attribs,
                   accessors);
  ReplaceAccessors(isolate(), map, factory()->caller_string(), rw_attribs,
                   accessors);
}

static void AddToWeakNativeContextList(Isolate* isolate, Context context) {
  DCHECK(context.IsNativeContext());
  Heap* heap = isolate->heap();
#ifdef DEBUG
  {  // NOLINT
    DCHECK(context.next_context_link().IsUndefined(isolate));
    // Check that context is not in the list yet.
    for (Object current = heap->native_contexts_list();
         !current.IsUndefined(isolate);
         current = Context::cast(current).next_context_link()) {
      DCHECK(current != context);
    }
  }
#endif
  context.set(Context::NEXT_CONTEXT_LINK, heap->native_contexts_list(),
              UPDATE_WEAK_WRITE_BARRIER);
  heap->set_native_contexts_list(context);
}

void Genesis::CreateRoots() {
  // Allocate the native context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // native context).
  native_context_ = factory()->NewNativeContext();

  AddToWeakNativeContextList(isolate(), *native_context());
  isolate()->set_context(*native_context());

  // Allocate the message listeners object.
  {
    Handle<TemplateList> list = TemplateList::New(isolate(), 1);
    native_context()->set_message_listeners(*list);
  }
}

void Genesis::InstallGlobalThisBinding() {
  Handle<ScriptContextTable> script_contexts(
      native_context()->script_context_table(), isolate());
  Handle<ScopeInfo> scope_info =
      ReadOnlyRoots(isolate()).global_this_binding_scope_info_handle();
  Handle<Context> context =
      factory()->NewScriptContext(native_context(), scope_info);

  // Go ahead and hook it up while we're at it.
  int slot = scope_info->ReceiverContextSlotIndex();
  DCHECK_EQ(slot, Context::MIN_CONTEXT_SLOTS);
  context->set(slot, native_context()->global_proxy());

  Handle<ScriptContextTable> new_script_contexts =
      ScriptContextTable::Extend(script_contexts, context);
  native_context()->set_script_context_table(*new_script_contexts);
}

Handle<JSGlobalObject> Genesis::CreateNewGlobals(
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    Handle<JSGlobalProxy> global_proxy) {
  // The argument global_proxy_template aka data is an ObjectTemplateInfo.
  // It has a constructor pointer that points at global_constructor which is a
  // FunctionTemplateInfo.
  // The global_proxy_constructor is used to (re)initialize the
  // global_proxy. The global_proxy_constructor also has a prototype_template
  // pointer that points at js_global_object_template which is an
  // ObjectTemplateInfo.
  // That in turn has a constructor pointer that points at
  // js_global_object_constructor which is a FunctionTemplateInfo.
  // js_global_object_constructor is used to make js_global_object_function
  // js_global_object_function is used to make the new global_object.
  //
  // --- G l o b a l ---
  // Step 1: Create a fresh JSGlobalObject.
  Handle<JSFunction> js_global_object_function;
  Handle<ObjectTemplateInfo> js_global_object_template;
  if (!global_proxy_template.IsEmpty()) {
    // Get prototype template of the global_proxy_template.
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor =
        Handle<FunctionTemplateInfo>(
            FunctionTemplateInfo::cast(data->constructor()), isolate());
    Handle<Object> proto_template(global_constructor->GetPrototypeTemplate(),
                                  isolate());
    if (!proto_template->IsUndefined(isolate())) {
      js_global_object_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_object_template.is_null()) {
    Handle<String> name = factory()->empty_string();
    Handle<JSObject> prototype =
        factory()->NewFunctionPrototype(isolate()->object_function());
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        name, prototype, JS_GLOBAL_OBJECT_TYPE, JSGlobalObject::kHeaderSize, 0,
        Builtins::kIllegal, MUTABLE);
    js_global_object_function = factory()->NewFunction(args);
#ifdef DEBUG
    LookupIterator it(isolate(), prototype, factory()->constructor_string(),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Handle<Object> value = Object::GetProperty(&it).ToHandleChecked();
    DCHECK(it.IsFound());
    DCHECK_EQ(*isolate()->object_function(), *value);
#endif
  } else {
    Handle<FunctionTemplateInfo> js_global_object_constructor(
        FunctionTemplateInfo::cast(js_global_object_template->constructor()),
        isolate());
    js_global_object_function = ApiNatives::CreateApiFunction(
        isolate(), isolate()->native_context(), js_global_object_constructor,
        factory()->the_hole_value(), JS_GLOBAL_OBJECT_TYPE);
  }

  js_global_object_function->initial_map().set_is_prototype_map(true);
  js_global_object_function->initial_map().set_is_dictionary_map(true);
  js_global_object_function->initial_map().set_may_have_interesting_symbols(
      true);
  Handle<JSGlobalObject> global_object =
      factory()->NewJSGlobalObject(js_global_object_function);

  // Step 2: (re)initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_proxy_template.IsEmpty()) {
    Handle<String> name = factory()->empty_string();
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        name, factory()->the_hole_value(), JS_GLOBAL_PROXY_TYPE,
        JSGlobalProxy::SizeWithEmbedderFields(0), 0, Builtins::kIllegal,
        MUTABLE);
    global_proxy_function = factory()->NewFunction(args);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor(
        FunctionTemplateInfo::cast(data->constructor()), isolate());
    global_proxy_function = ApiNatives::CreateApiFunction(
        isolate(), isolate()->native_context(), global_constructor,
        factory()->the_hole_value(), JS_GLOBAL_PROXY_TYPE);
  }
  global_proxy_function->initial_map().set_is_access_check_needed(true);
  global_proxy_function->initial_map().set_may_have_interesting_symbols(true);
  native_context()->set_global_proxy_function(*global_proxy_function);

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);

  // Set the native context for the global object.
  global_object->set_native_context(*native_context());
  global_object->set_global_proxy(*global_proxy);
  // Set the native context of the global proxy.
  global_proxy->set_native_context(*native_context());
  // Set the global proxy of the native context. If the native context has been
  // deserialized, the global proxy is already correctly set up by the
  // deserializer. Otherwise it's undefined.
  DCHECK(native_context()
             ->get(Context::GLOBAL_PROXY_INDEX)
             .IsUndefined(isolate()) ||
         native_context()->global_proxy_object() == *global_proxy);
  native_context()->set_global_proxy_object(*global_proxy);

  return global_object;
}

void Genesis::HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy) {
  // Re-initialize the global proxy with the global proxy function from the
  // snapshot, and then set up the link to the native context.
  Handle<JSFunction> global_proxy_function(
      native_context()->global_proxy_function(), isolate());
  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()), isolate());
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->set_native_context(*native_context());
  DCHECK(native_context()->global_proxy() == *global_proxy);
}

void Genesis::HookUpGlobalObject(Handle<JSGlobalObject> global_object) {
  Handle<JSGlobalObject> global_object_from_snapshot(
      JSGlobalObject::cast(native_context()->extension()), isolate());
  native_context()->set_extension(*global_object);
  native_context()->set_security_token(*global_object);

  TransferNamedProperties(global_object_from_snapshot, global_object);
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}

static void InstallWithIntrinsicDefaultProto(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             int context_index) {
  Handle<Smi> index(Smi::FromInt(context_index), isolate);
  JSObject::AddProperty(isolate, function,
                        isolate->factory()->native_context_index_symbol(),
                        index, NONE);
  isolate->native_context()->set(context_index, *function);
}

static void InstallError(Isolate* isolate, Handle<JSObject> global,
                         Handle<String> name, int context_index) {
  Factory* factory = isolate->factory();

  // Most Error objects consist of a message and a stack trace.
  // Reserve two in-object properties for these.
  const int kInObjectPropertiesCount = 2;
  const int kErrorObjectSize =
      JSObject::kHeaderSize + kInObjectPropertiesCount * kTaggedSize;
  Handle<JSFunction> error_fun =
      InstallFunction(isolate, global, name, JS_ERROR_TYPE, kErrorObjectSize,
                      kInObjectPropertiesCount, factory->the_hole_value(),
                      Builtins::kErrorConstructor);
  error_fun->shared().DontAdaptArguments();
  error_fun->shared().set_length(1);

  if (context_index == Context::ERROR_FUNCTION_INDEX) {
    SimpleInstallFunction(isolate, error_fun, "captureStackTrace",
                          Builtins::kErrorCaptureStackTrace, 2, false);
  }

  InstallWithIntrinsicDefaultProto(isolate, error_fun, context_index);

  {
    // Setup %XXXErrorPrototype%.
    Handle<JSObject> prototype(JSObject::cast(error_fun->instance_prototype()),
                               isolate);

    JSObject::AddProperty(isolate, prototype, factory->name_string(), name,
                          DONT_ENUM);
    JSObject::AddProperty(isolate, prototype, factory->message_string(),
                          factory->empty_string(), DONT_ENUM);

    if (context_index == Context::ERROR_FUNCTION_INDEX) {
      Handle<JSFunction> to_string_fun =
          SimpleInstallFunction(isolate, prototype, "toString",
                                Builtins::kErrorPrototypeToString, 0, true);
      isolate->native_context()->set_error_to_string(*to_string_fun);
      isolate->native_context()->set_initial_error_prototype(*prototype);
    } else {
      Handle<JSFunction> global_error = isolate->error_function();
      CHECK(JSReceiver::SetPrototype(error_fun, global_error, false,
                                     kThrowOnError)
                .FromMaybe(false));
      CHECK(JSReceiver::SetPrototype(prototype,
                                     handle(global_error->prototype(), isolate),
                                     false, kThrowOnError)
                .FromMaybe(false));
    }
  }

  Handle<Map> initial_map(error_fun->initial_map(), isolate);
  Map::EnsureDescriptorSlack(isolate, initial_map, 1);

  {
    Handle<AccessorInfo> info = factory->error_stack_accessor();
    Descriptor d = Descriptor::AccessorConstant(handle(info->name(), isolate),
                                                info, DONT_ENUM);
    initial_map->AppendDescriptor(isolate, &d);
  }
}

namespace {

void InstallMakeError(Isolate* isolate, int builtin_id, int context_index) {
  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
      isolate->factory()->empty_string(), isolate->factory()->the_hole_value(),
      JS_OBJECT_TYPE, JSObject::kHeaderSize, 0, builtin_id, MUTABLE);

  Handle<JSFunction> function = isolate->factory()->NewFunction(args);
  function->shared().DontAdaptArguments();
  isolate->native_context()->set(context_index, *function);
}

}  // namespace

// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpGlobalObject.
void Genesis::InitializeGlobal(Handle<JSGlobalObject> global_object,
                               Handle<JSFunction> empty_function) {
  // --- N a t i v e   C o n t e x t ---
  native_context()->set_previous(Context());
  // Set extension and global object.
  native_context()->set_extension(*global_object);
  // Security setup: Set the security token of the native context to the global
  // object. This makes the security check between two different contexts fail
  // by default even in case of global object reinitialization.
  native_context()->set_security_token(*global_object);

  Factory* factory = isolate_->factory();

  {  // -- C o n t e x t
    Handle<Map> map =
        factory->NewMap(FUNCTION_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_function_context_map(*map);

    map = factory->NewMap(CATCH_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_catch_context_map(*map);

    map = factory->NewMap(WITH_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_with_context_map(*map);

    map = factory->NewMap(DEBUG_EVALUATE_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_debug_evaluate_context_map(*map);

    map = factory->NewMap(BLOCK_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_block_context_map(*map);

    map = factory->NewMap(MODULE_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_module_context_map(*map);

    map = factory->NewMap(AWAIT_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_await_context_map(*map);

    map = factory->NewMap(SCRIPT_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_script_context_map(*map);

    map = factory->NewMap(EVAL_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_eval_context_map(*map);

    Handle<ScriptContextTable> script_context_table =
        factory->NewScriptContextTable();
    native_context()->set_script_context_table(*script_context_table);
    InstallGlobalThisBinding();
  }

  {  // --- O b j e c t ---
    Handle<String> object_name = factory->Object_string();
    Handle<JSFunction> object_function = isolate_->object_function();
    JSObject::AddProperty(isolate_, global_object, object_name, object_function,
                          DONT_ENUM);

    SimpleInstallFunction(isolate_, object_function, "assign",
                          Builtins::kObjectAssign, 2, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyDescriptor",
                          Builtins::kObjectGetOwnPropertyDescriptor, 2, false);
    SimpleInstallFunction(isolate_, object_function,
                          "getOwnPropertyDescriptors",
                          Builtins::kObjectGetOwnPropertyDescriptors, 1, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyNames",
                          Builtins::kObjectGetOwnPropertyNames, 1, true);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertySymbols",
                          Builtins::kObjectGetOwnPropertySymbols, 1, false);
    SimpleInstallFunction(isolate_, object_function, "is", Builtins::kObjectIs,
                          2, true);
    SimpleInstallFunction(isolate_, object_function, "preventExtensions",
                          Builtins::kObjectPreventExtensions, 1, true);
    SimpleInstallFunction(isolate_, object_function, "seal",
                          Builtins::kObjectSeal, 1, false);

    Handle<JSFunction> object_create = SimpleInstallFunction(
        isolate_, object_function, "create", Builtins::kObjectCreate, 2, false);
    native_context()->set_object_create(*object_create);

    SimpleInstallFunction(isolate_, object_function, "defineProperties",
                          Builtins::kObjectDefineProperties, 2, true);

    SimpleInstallFunction(isolate_, object_function, "defineProperty",
                          Builtins::kObjectDefineProperty, 3, true);

    SimpleInstallFunction(isolate_, object_function, "freeze",
                          Builtins::kObjectFreeze, 1, false);

    SimpleInstallFunction(isolate_, object_function, "getPrototypeOf",
                          Builtins::kObjectGetPrototypeOf, 1, true);
    SimpleInstallFunction(isolate_, object_function, "setPrototypeOf",
                          Builtins::kObjectSetPrototypeOf, 2, true);

    SimpleInstallFunction(isolate_, object_function, "isExtensible",
                          Builtins::kObjectIsExtensible, 1, true);
    SimpleInstallFunction(isolate_, object_function, "isFrozen",
                          Builtins::kObjectIsFrozen, 1, false);

    SimpleInstallFunction(isolate_, object_function, "isSealed",
                          Builtins::kObjectIsSealed, 1, false);

    SimpleInstallFunction(isolate_, object_function, "keys",
                          Builtins::kObjectKeys, 1, true);
    SimpleInstallFunction(isolate_, object_function, "entries",
                          Builtins::kObjectEntries, 1, true);
    SimpleInstallFunction(isolate_, object_function, "fromEntries",
                          Builtins::kObjectFromEntries, 1, false);
    SimpleInstallFunction(isolate_, object_function, "values",
                          Builtins::kObjectValues, 1, true);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineGetter__", Builtins::kObjectDefineGetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineSetter__", Builtins::kObjectDefineSetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "hasOwnProperty",
                          Builtins::kObjectPrototypeHasOwnProperty, 1, true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupGetter__", Builtins::kObjectLookupGetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupSetter__", Builtins::kObjectLookupSetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "isPrototypeOf",
                          Builtins::kObjectPrototypeIsPrototypeOf, 1, true);
    SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "propertyIsEnumerable",
        Builtins::kObjectPrototypePropertyIsEnumerable, 1, false);
    Handle<JSFunction> object_to_string = SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "toString",
        Builtins::kObjectPrototypeToString, 0, true);
    native_context()->set_object_to_string(*object_to_string);
    Handle<JSFunction> object_value_of = SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "valueOf",
        Builtins::kObjectPrototypeValueOf, 0, true);
    native_context()->set_object_value_of_function(*object_value_of);

    SimpleInstallGetterSetter(
        isolate_, isolate_->initial_object_prototype(), factory->proto_string(),
        Builtins::kObjectPrototypeGetProto, Builtins::kObjectPrototypeSetProto);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "toLocaleString",
                          Builtins::kObjectPrototypeToLocaleString, 0, true);
  }

  Handle<JSObject> global(native_context()->global_object(), isolate());

  {  // --- F u n c t i o n ---
    Handle<JSFunction> prototype = empty_function;
    Handle<JSFunction> function_fun =
        InstallFunction(isolate_, global, "Function", JS_FUNCTION_TYPE,
                        JSFunction::kSizeWithPrototype, 0, prototype,
                        Builtins::kFunctionConstructor);
    // Function instances are sloppy by default.
    function_fun->set_prototype_or_initial_map(
        *isolate_->sloppy_function_map());
    function_fun->shared().DontAdaptArguments();
    function_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, function_fun,
                                     Context::FUNCTION_FUNCTION_INDEX);

    // Setup the methods on the %FunctionPrototype%.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          function_fun, DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "apply",
                          Builtins::kFunctionPrototypeApply, 2, false);
    SimpleInstallFunction(isolate_, prototype, "bind",
                          Builtins::kFastFunctionPrototypeBind, 1, false);
    SimpleInstallFunction(isolate_, prototype, "call",
                          Builtins::kFunctionPrototypeCall, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kFunctionPrototypeToString, 0, false);

    // Install the @@hasInstance function.
    Handle<JSFunction> has_instance = InstallFunctionAtSymbol(
        isolate_, prototype, factory->has_instance_symbol(),
        "[Symbol.hasInstance]", Builtins::kFunctionPrototypeHasInstance, 1,
        true,
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY));
    native_context()->set_function_has_instance(*has_instance);

    // Complete setting up function maps.
    {
      isolate_->sloppy_function_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_name_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->strict_function_map()->SetConstructor(*function_fun);
      isolate_->strict_function_with_name_map()->SetConstructor(*function_fun);
      strict_function_with_home_object_map_->SetConstructor(*function_fun);
      strict_function_with_name_and_home_object_map_->SetConstructor(
          *function_fun);
      isolate_->strict_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->class_function_map()->SetConstructor(*function_fun);
    }
  }

  {  // --- A s y n c F r o m S y n c I t e r a t o r
    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncIteratorValueUnwrap, factory->empty_string(),
        1);
    native_context()->set_async_iterator_value_unwrap_shared_fun(*info);
  }

  {  // --- A s y n c G e n e r a t o r ---
    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorAwaitResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_await_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorAwaitRejectClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_await_reject_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorYieldResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_yield_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnClosedResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_closed_resolve_shared_fun(
        *info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnClosedRejectClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_closed_reject_shared_fun(
        *info);
  }

  Handle<JSFunction> array_prototype_to_string_fun;
  {  // --- A r r a y ---
    Handle<JSFunction> array_function = InstallFunction(
        isolate_, global, "Array", JS_ARRAY_TYPE, JSArray::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtins::kArrayConstructor);
    array_function->shared().DontAdaptArguments();

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared().set_length(1);

    Handle<Map> initial_map(array_function->initial_map(), isolate());

    // This assert protects an optimization in
    // HGraphBuilder::JSArrayBuilder::EmitMapCode()
    DCHECK(initial_map->elements_kind() == GetInitialFastElementsKind());
    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);

    STATIC_ASSERT(JSArray::kLengthDescriptorIndex == 0);
    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->array_length_accessor(), attribs);
      initial_map->AppendDescriptor(isolate(), &d);
    }

    InstallWithIntrinsicDefaultProto(isolate_, array_function,
                                     Context::ARRAY_FUNCTION_INDEX);
    InstallSpeciesGetter(isolate_, array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(isolate_, native_context(), initial_map);

    // Set up %ArrayPrototype%.
    // The %ArrayPrototype% has TERMINAL_FAST_ELEMENTS_KIND in order to ensure
    // that constant functions stay constant after turning prototype to setup
    // mode and back.
    Handle<JSArray> proto = factory->NewJSArray(0, TERMINAL_FAST_ELEMENTS_KIND,
                                                AllocationType::kOld);
    JSFunction::SetPrototype(array_function, proto);
    native_context()->set_initial_array_prototype(*proto);

    SimpleInstallFunction(isolate_, array_function, "isArray",
                          Builtins::kArrayIsArray, 1, true);
    SimpleInstallFunction(isolate_, array_function, "from",
                          Builtins::kArrayFrom, 1, false);
    SimpleInstallFunction(isolate_, array_function, "of", Builtins::kArrayOf, 0,
                          false);

    JSObject::AddProperty(isolate_, proto, factory->constructor_string(),
                          array_function, DONT_ENUM);

    SimpleInstallFunction(isolate_, proto, "concat", Builtins::kArrayConcat, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "copyWithin",
                          Builtins::kArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, proto, "fill",
                          Builtins::kArrayPrototypeFill, 1, false);
    SimpleInstallFunction(isolate_, proto, "find",
                          Builtins::kArrayPrototypeFind, 1, false);
    SimpleInstallFunction(isolate_, proto, "findIndex",
                          Builtins::kArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, proto, "lastIndexOf",
                          Builtins::kArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, proto, "pop", Builtins::kArrayPrototypePop,
                          0, false);
    SimpleInstallFunction(isolate_, proto, "push",
                          Builtins::kArrayPrototypePush, 1, false);
    SimpleInstallFunction(isolate_, proto, "reverse",
                          Builtins::kArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, proto, "shift",
                          Builtins::kArrayPrototypeShift, 0, false);
    SimpleInstallFunction(isolate_, proto, "unshift",
                          Builtins::kArrayPrototypeUnshift, 1, false);
    SimpleInstallFunction(isolate_, proto, "slice",
                          Builtins::kArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, proto, "sort",
                          Builtins::kArrayPrototypeSort, 1, false);
    SimpleInstallFunction(isolate_, proto, "splice",
                          Builtins::kArrayPrototypeSplice, 2, false);
    SimpleInstallFunction(isolate_, proto, "includes", Builtins::kArrayIncludes,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "indexOf", Builtins::kArrayIndexOf,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "join",
                          Builtins::kArrayPrototypeJoin, 1, false);

    {  // Set up iterator-related properties.
      Handle<JSFunction> keys = InstallFunctionWithBuiltinId(
          isolate_, proto, "keys", Builtins::kArrayPrototypeKeys, 0, true);
      native_context()->set_array_keys_iterator(*keys);

      Handle<JSFunction> entries = InstallFunctionWithBuiltinId(
          isolate_, proto, "entries", Builtins::kArrayPrototypeEntries, 0,
          true);
      native_context()->set_array_entries_iterator(*entries);

      Handle<JSFunction> values = InstallFunctionWithBuiltinId(
          isolate_, proto, "values", Builtins::kArrayPrototypeValues, 0, true);
      JSObject::AddProperty(isolate_, proto, factory->iterator_symbol(), values,
                            DONT_ENUM);
      native_context()->set_array_values_iterator(*values);
    }

    Handle<JSFunction> for_each_fun = SimpleInstallFunction(
        isolate_, proto, "forEach", Builtins::kArrayForEach, 1, false);
    native_context()->set_array_for_each_iterator(*for_each_fun);
    SimpleInstallFunction(isolate_, proto, "filter", Builtins::kArrayFilter, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "flat",
                          Builtins::kArrayPrototypeFlat, 0, false);
    SimpleInstallFunction(isolate_, proto, "flatMap",
                          Builtins::kArrayPrototypeFlatMap, 1, false);
    SimpleInstallFunction(isolate_, proto, "map", Builtins::kArrayMap, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "every", Builtins::kArrayEvery, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "some", Builtins::kArraySome, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduce", Builtins::kArrayReduce, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduceRight",
                          Builtins::kArrayReduceRight, 1, false);
    SimpleInstallFunction(isolate_, proto, "toLocaleString",
                          Builtins::kArrayPrototypeToLocaleString, 0, false);
    array_prototype_to_string_fun =
        SimpleInstallFunction(isolate_, proto, "toString",
                              Builtins::kArrayPrototypeToString, 0, false);

    Handle<JSObject> unscopables = factory->NewJSObjectWithNullProto();
    InstallTrueValuedProperty(isolate_, unscopables, "copyWithin");
    InstallTrueValuedProperty(isolate_, unscopables, "entries");
    InstallTrueValuedProperty(isolate_, unscopables, "fill");
    InstallTrueValuedProperty(isolate_, unscopables, "find");
    InstallTrueValuedProperty(isolate_, unscopables, "findIndex");
    InstallTrueValuedProperty(isolate_, unscopables, "flat");
    InstallTrueValuedProperty(isolate_, unscopables, "flatMap");
    InstallTrueValuedProperty(isolate_, unscopables, "includes");
    InstallTrueValuedProperty(isolate_, unscopables, "keys");
    InstallTrueValuedProperty(isolate_, unscopables, "values");
    JSObject::MigrateSlowToFast(unscopables, 0, "Bootstrapping");
    JSObject::AddProperty(
        isolate_, proto, factory->unscopables_symbol(), unscopables,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<Map> map(proto->map(), isolate_);
    Map::SetShouldBeFastPrototypeMap(map, true, isolate_);
  }

  {  // --- A r r a y I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> array_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(array_iterator_prototype, iterator_prototype);

    InstallToStringTag(isolate_, array_iterator_prototype,
                       factory->ArrayIterator_string());

    InstallFunctionWithBuiltinId(isolate_, array_iterator_prototype, "next",
                                 Builtins::kArrayIteratorPrototypeNext, 0,
                                 true);

    Handle<JSFunction> array_iterator_function =
        CreateFunction(isolate_, factory->ArrayIterator_string(),
                       JS_ARRAY_ITERATOR_TYPE, JSArrayIterator::kHeaderSize, 0,
                       array_iterator_prototype, Builtins::kIllegal);
    array_iterator_function->shared().set_native(false);

    native_context()->set_initial_array_iterator_map(
        array_iterator_function->initial_map());
    native_context()->set_initial_array_iterator_prototype(
        *array_iterator_prototype);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun = InstallFunction(
        isolate_, global, "Number", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtins::kNumberConstructor);
    number_fun->shared().DontAdaptArguments();
    number_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, number_fun,
                                     Context::NUMBER_FUNCTION_INDEX);

    // Create the %NumberPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(number_fun, AllocationType::kOld));
    prototype->set_value(Smi::zero());
    JSFunction::SetPrototype(number_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          number_fun, DONT_ENUM);

    // Install the Number.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toExponential",
                          Builtins::kNumberPrototypeToExponential, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toFixed",
                          Builtins::kNumberPrototypeToFixed, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toPrecision",
                          Builtins::kNumberPrototypeToPrecision, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kNumberPrototypeToString, 1, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kNumberPrototypeValueOf, 0, true);

    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kNumberPrototypeToLocaleString, 0, false);

    // Install the Number functions.
    SimpleInstallFunction(isolate_, number_fun, "isFinite",
                          Builtins::kNumberIsFinite, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isInteger",
                          Builtins::kNumberIsInteger, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isNaN", Builtins::kNumberIsNaN,
                          1, true);
    SimpleInstallFunction(isolate_, number_fun, "isSafeInteger",
                          Builtins::kNumberIsSafeInteger, 1, true);

    // Install Number.parseFloat and Global.parseFloat.
    Handle<JSFunction> parse_float_fun =
        SimpleInstallFunction(isolate_, number_fun, "parseFloat",
                              Builtins::kNumberParseFloat, 1, true);
    JSObject::AddProperty(isolate_, global_object, "parseFloat",
                          parse_float_fun, DONT_ENUM);

    // Install Number.parseInt and Global.parseInt.
    Handle<JSFunction> parse_int_fun = SimpleInstallFunction(
        isolate_, number_fun, "parseInt", Builtins::kNumberParseInt, 2, true);
    JSObject::AddProperty(isolate_, global_object, "parseInt", parse_int_fun,
                          DONT_ENUM);

    // Install Number constants
    const double kMaxValue = 1.7976931348623157e+308;
    const double kMinValue = 5e-324;
    const double kMinSafeInteger = -kMaxSafeInteger;
    const double kEPS = 2.220446049250313e-16;

    InstallConstant(isolate_, number_fun, "MAX_VALUE",
                    factory->NewNumber(kMaxValue));
    InstallConstant(isolate_, number_fun, "MIN_VALUE",
                    factory->NewNumber(kMinValue));
    InstallConstant(isolate_, number_fun, "NaN", factory->nan_value());
    InstallConstant(isolate_, number_fun, "NEGATIVE_INFINITY",
                    factory->NewNumber(-V8_INFINITY));
    InstallConstant(isolate_, number_fun, "POSITIVE_INFINITY",
                    factory->infinity_value());
    InstallConstant(isolate_, number_fun, "MAX_SAFE_INTEGER",
                    factory->NewNumber(kMaxSafeInteger));
    InstallConstant(isolate_, number_fun, "MIN_SAFE_INTEGER",
                    factory->NewNumber(kMinSafeInteger));
    InstallConstant(isolate_, number_fun, "EPSILON", factory->NewNumber(kEPS));

    InstallConstant(isolate_, global, "Infinity", factory->infinity_value());
    InstallConstant(isolate_, global, "NaN", factory->nan_value());
    InstallConstant(isolate_, global, "undefined", factory->undefined_value());
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun = InstallFunction(
        isolate_, global, "Boolean", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtins::kBooleanConstructor);
    boolean_fun->shared().DontAdaptArguments();
    boolean_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, boolean_fun,
                                     Context::BOOLEAN_FUNCTION_INDEX);

    // Create the %BooleanPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(boolean_fun, AllocationType::kOld));
    prototype->set_value(ReadOnlyRoots(isolate_).false_value());
    JSFunction::SetPrototype(boolean_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          boolean_fun, DONT_ENUM);

    // Install the Boolean.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kBooleanPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kBooleanPrototypeValueOf, 0, true);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun = InstallFunction(
        isolate_, global, "String", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtins::kStringConstructor);
    string_fun->shared().DontAdaptArguments();
    string_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, string_fun,
                                     Context::STRING_FUNCTION_INDEX);

    Handle<Map> string_map = Handle<Map>(
        native_context()->string_function().initial_map(), isolate());
    string_map->set_elements_kind(FAST_STRING_WRAPPER_ELEMENTS);
    Map::EnsureDescriptorSlack(isolate_, string_map, 1);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->string_length_accessor(), attribs);
      string_map->AppendDescriptor(isolate(), &d);
    }

    // Install the String.fromCharCode function.
    SimpleInstallFunction(isolate_, string_fun, "fromCharCode",
                          Builtins::kStringFromCharCode, 1, false);

    // Install the String.fromCodePoint function.
    SimpleInstallFunction(isolate_, string_fun, "fromCodePoint",
                          Builtins::kStringFromCodePoint, 1, false);

    // Install the String.raw function.
    SimpleInstallFunction(isolate_, string_fun, "raw", Builtins::kStringRaw, 1,
                          false);

    // Create the %StringPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(string_fun, AllocationType::kOld));
    prototype->set_value(ReadOnlyRoots(isolate_).empty_string());
    JSFunction::SetPrototype(string_fun, prototype);
    native_context()->set_initial_string_prototype(*prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          string_fun, DONT_ENUM);

    // Install the String.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "anchor",
                          Builtins::kStringPrototypeAnchor, 1, false);
    SimpleInstallFunction(isolate_, prototype, "big",
                          Builtins::kStringPrototypeBig, 0, false);
    SimpleInstallFunction(isolate_, prototype, "blink",
                          Builtins::kStringPrototypeBlink, 0, false);
    SimpleInstallFunction(isolate_, prototype, "bold",
                          Builtins::kStringPrototypeBold, 0, false);
    SimpleInstallFunction(isolate_, prototype, "charAt",
                          Builtins::kStringPrototypeCharAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "charCodeAt",
                          Builtins::kStringPrototypeCharCodeAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "codePointAt",
                          Builtins::kStringPrototypeCodePointAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "concat",
                          Builtins::kStringPrototypeConcat, 1, false);
    SimpleInstallFunction(isolate_, prototype, "endsWith",
                          Builtins::kStringPrototypeEndsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fontcolor",
                          Builtins::kStringPrototypeFontcolor, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fontsize",
                          Builtins::kStringPrototypeFontsize, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fixed",
                          Builtins::kStringPrototypeFixed, 0, false);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtins::kStringPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtins::kStringPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "italics",
                          Builtins::kStringPrototypeItalics, 0, false);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtins::kStringPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "link",
                          Builtins::kStringPrototypeLink, 1, false);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtins::kStringPrototypeLocaleCompare, 1, false);
#else
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtins::kStringPrototypeLocaleCompare, 1, true);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "match",
                          Builtins::kStringPrototypeMatch, 1, true);
    SimpleInstallFunction(isolate_, prototype, "matchAll",
                          Builtins::kStringPrototypeMatchAll, 1, true);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtins::kStringPrototypeNormalizeIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtins::kStringPrototypeNormalize, 0, false);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "padEnd",
                          Builtins::kStringPrototypePadEnd, 1, false);
    SimpleInstallFunction(isolate_, prototype, "padStart",
                          Builtins::kStringPrototypePadStart, 1, false);
    SimpleInstallFunction(isolate_, prototype, "repeat",
                          Builtins::kStringPrototypeRepeat, 1, true);
    SimpleInstallFunction(isolate_, prototype, "replace",
                          Builtins::kStringPrototypeReplace, 2, true);
    SimpleInstallFunction(isolate_, prototype, "search",
                          Builtins::kStringPrototypeSearch, 1, true);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtins::kStringPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "small",
                          Builtins::kStringPrototypeSmall, 0, false);
    SimpleInstallFunction(isolate_, prototype, "split",
                          Builtins::kStringPrototypeSplit, 2, false);
    SimpleInstallFunction(isolate_, prototype, "strike",
                          Builtins::kStringPrototypeStrike, 0, false);
    SimpleInstallFunction(isolate_, prototype, "sub",
                          Builtins::kStringPrototypeSub, 0, false);
    SimpleInstallFunction(isolate_, prototype, "substr",
                          Builtins::kStringPrototypeSubstr, 2, false);
    SimpleInstallFunction(isolate_, prototype, "substring",
                          Builtins::kStringPrototypeSubstring, 2, false);
    SimpleInstallFunction(isolate_, prototype, "sup",
                          Builtins::kStringPrototypeSup, 0, false);
    SimpleInstallFunction(isolate_, prototype, "startsWith",
                          Builtins::kStringPrototypeStartsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kStringPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "trim",
                          Builtins::kStringPrototypeTrim, 0, false);

    // Install `String.prototype.trimStart` with `trimLeft` alias.
    Handle<JSFunction> trim_start_fun =
        SimpleInstallFunction(isolate_, prototype, "trimStart",
                              Builtins::kStringPrototypeTrimStart, 0, false);
    JSObject::AddProperty(isolate_, prototype, "trimLeft", trim_start_fun,
                          DONT_ENUM);

    // Install `String.prototype.trimEnd` with `trimRight` alias.
    Handle<JSFunction> trim_end_fun =
        SimpleInstallFunction(isolate_, prototype, "trimEnd",
                              Builtins::kStringPrototypeTrimEnd, 0, false);
    JSObject::AddProperty(isolate_, prototype, "trimRight", trim_end_fun,
                          DONT_ENUM);

    SimpleInstallFunction(isolate_, prototype, "toLocaleLowerCase",
                          Builtins::kStringPrototypeToLocaleLowerCase, 0,
                          false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleUpperCase",
                          Builtins::kStringPrototypeToLocaleUpperCase, 0,
                          false);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtins::kStringPrototypeToLowerCaseIntl, 0, true);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtins::kStringPrototypeToUpperCaseIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtins::kStringPrototypeToLowerCase, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtins::kStringPrototypeToUpperCase, 0, false);
#endif
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kStringPrototypeValueOf, 0, true);

    InstallFunctionAtSymbol(
        isolate_, prototype, factory->iterator_symbol(), "[Symbol.iterator]",
        Builtins::kStringPrototypeIterator, 0, true, DONT_ENUM);
  }

  {  // --- S t r i n g I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> string_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(string_iterator_prototype, iterator_prototype);

    InstallToStringTag(isolate_, string_iterator_prototype, "String Iterator");

    InstallFunctionWithBuiltinId(isolate_, string_iterator_prototype, "next",
                                 Builtins::kStringIteratorPrototypeNext, 0,
                                 true);

    Handle<JSFunction> string_iterator_function = CreateFunction(
        isolate_, factory->InternalizeUtf8String("StringIterator"),
        JS_STRING_ITERATOR_TYPE, JSStringIterator::kHeaderSize, 0,
        string_iterator_prototype, Builtins::kIllegal);
    string_iterator_function->shared().set_native(false);
    native_context()->set_initial_string_iterator_map(
        string_iterator_function->initial_map());
    native_context()->set_initial_string_iterator_prototype(
        *string_iterator_prototype);
  }

  {  // --- S y m b o l ---
    Handle<JSFunction> symbol_fun = InstallFunction(
        isolate_, global, "Symbol", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0, factory->the_hole_value(),
        Builtins::kSymbolConstructor);
    symbol_fun->shared().set_length(0);
    symbol_fun->shared().DontAdaptArguments();
    native_context()->set_symbol_function(*symbol_fun);

    // Install the Symbol.for and Symbol.keyFor functions.
    SimpleInstallFunction(isolate_, symbol_fun, "for", Builtins::kSymbolFor, 1,
                          false);
    SimpleInstallFunction(isolate_, symbol_fun, "keyFor",
                          Builtins::kSymbolKeyFor, 1, false);

    // Install well-known symbols.
    InstallConstant(isolate_, symbol_fun, "asyncIterator",
                    factory->async_iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "hasInstance",
                    factory->has_instance_symbol());
    InstallConstant(isolate_, symbol_fun, "isConcatSpreadable",
                    factory->is_concat_spreadable_symbol());
    InstallConstant(isolate_, symbol_fun, "iterator",
                    factory->iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "match", factory->match_symbol());
    InstallConstant(isolate_, symbol_fun, "matchAll",
                    factory->match_all_symbol());
    InstallConstant(isolate_, symbol_fun, "replace", factory->replace_symbol());
    InstallConstant(isolate_, symbol_fun, "search", factory->search_symbol());
    InstallConstant(isolate_, symbol_fun, "species", factory->species_symbol());
    InstallConstant(isolate_, symbol_fun, "split", factory->split_symbol());
    InstallConstant(isolate_, symbol_fun, "toPrimitive",
                    factory->to_primitive_symbol());
    InstallConstant(isolate_, symbol_fun, "toStringTag",
                    factory->to_string_tag_symbol());
    InstallConstant(isolate_, symbol_fun, "unscopables",
                    factory->unscopables_symbol());

    // Setup %SymbolPrototype%.
    Handle<JSObject> prototype(JSObject::cast(symbol_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, "Symbol");

    // Install the Symbol.prototype methods.
    InstallFunctionWithBuiltinId(isolate_, prototype, "toString",
                                 Builtins::kSymbolPrototypeToString, 0, true);
    InstallFunctionWithBuiltinId(isolate_, prototype, "valueOf",
                                 Builtins::kSymbolPrototypeValueOf, 0, true);

    // Install the Symbol.prototype.description getter.
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("description"),
                        Builtins::kSymbolPrototypeDescriptionGetter, true);

    // Install the @@toPrimitive function.
    InstallFunctionAtSymbol(
        isolate_, prototype, factory->to_primitive_symbol(),
        "[Symbol.toPrimitive]", Builtins::kSymbolPrototypeToPrimitive, 1, true,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // --- D a t e ---
    Handle<JSFunction> date_fun = InstallFunction(
        isolate_, global, "Date", JS_DATE_TYPE, JSDate::kHeaderSize, 0,
        factory->the_hole_value(), Builtins::kDateConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, date_fun,
                                     Context::DATE_FUNCTION_INDEX);
    date_fun->shared().set_length(7);
    date_fun->shared().DontAdaptArguments();

    // Install the Date.now, Date.parse and Date.UTC functions.
    SimpleInstallFunction(isolate_, date_fun, "now", Builtins::kDateNow, 0,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "parse", Builtins::kDateParse, 1,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "UTC", Builtins::kDateUTC, 7,
                          false);

    // Setup %DatePrototype%.
    Handle<JSObject> prototype(JSObject::cast(date_fun->instance_prototype()),
                               isolate());

    // Install the Date.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toISOString",
                          Builtins::kDatePrototypeToISOString, 0, false);
    Handle<JSFunction> to_utc_string =
        SimpleInstallFunction(isolate_, prototype, "toUTCString",
                              Builtins::kDatePrototypeToUTCString, 0, false);
    JSObject::AddProperty(isolate_, prototype, "toGMTString", to_utc_string,
                          DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "getDate",
                          Builtins::kDatePrototypeGetDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setDate",
                          Builtins::kDatePrototypeSetDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getDay",
                          Builtins::kDatePrototypeGetDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getFullYear",
                          Builtins::kDatePrototypeGetFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setFullYear",
                          Builtins::kDatePrototypeSetFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getHours",
                          Builtins::kDatePrototypeGetHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setHours",
                          Builtins::kDatePrototypeSetHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getMilliseconds",
                          Builtins::kDatePrototypeGetMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMilliseconds",
                          Builtins::kDatePrototypeSetMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getMinutes",
                          Builtins::kDatePrototypeGetMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMinutes",
                          Builtins::kDatePrototypeSetMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getMonth",
                          Builtins::kDatePrototypeGetMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMonth",
                          Builtins::kDatePrototypeSetMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getSeconds",
                          Builtins::kDatePrototypeGetSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setSeconds",
                          Builtins::kDatePrototypeSetSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getTime",
                          Builtins::kDatePrototypeGetTime, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setTime",
                          Builtins::kDatePrototypeSetTime, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getTimezoneOffset",
                          Builtins::kDatePrototypeGetTimezoneOffset, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCDate",
                          Builtins::kDatePrototypeGetUTCDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCDate",
                          Builtins::kDatePrototypeSetUTCDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCDay",
                          Builtins::kDatePrototypeGetUTCDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCFullYear",
                          Builtins::kDatePrototypeGetUTCFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCFullYear",
                          Builtins::kDatePrototypeSetUTCFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCHours",
                          Builtins::kDatePrototypeGetUTCHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCHours",
                          Builtins::kDatePrototypeSetUTCHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMilliseconds",
                          Builtins::kDatePrototypeGetUTCMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMilliseconds",
                          Builtins::kDatePrototypeSetUTCMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMinutes",
                          Builtins::kDatePrototypeGetUTCMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMinutes",
                          Builtins::kDatePrototypeSetUTCMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMonth",
                          Builtins::kDatePrototypeGetUTCMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMonth",
                          Builtins::kDatePrototypeSetUTCMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCSeconds",
                          Builtins::kDatePrototypeGetUTCSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCSeconds",
                          Builtins::kDatePrototypeSetUTCSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kDatePrototypeValueOf, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getYear",
                          Builtins::kDatePrototypeGetYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setYear",
                          Builtins::kDatePrototypeSetYear, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toJSON",
                          Builtins::kDatePrototypeToJson, 1, false);

#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kDatePrototypeToLocaleString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtins::kDatePrototypeToLocaleDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtins::kDatePrototypeToLocaleTimeString, 0, false);
#else
    // Install Intl fallback functions.
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);
#endif  // V8_INTL_SUPPORT

    // Install the @@toPrimitive function.
    InstallFunctionAtSymbol(
        isolate_, prototype, factory->to_primitive_symbol(),
        "[Symbol.toPrimitive]", Builtins::kDatePrototypeToPrimitive, 1, true,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {
    Handle<SharedFunctionInfo> info = SimpleCreateBuiltinSharedFunctionInfo(
        isolate_, Builtins::kPromiseGetCapabilitiesExecutor,
        factory->empty_string(), 2);
    native_context()->set_promise_get_capabilities_executor_shared_fun(*info);
  }

  {  // -- P r o m i s e
    Handle<JSFunction> promise_fun = InstallFunction(
        isolate_, global, "Promise", JS_PROMISE_TYPE,
        JSPromise::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtins::kPromiseConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, promise_fun,
                                     Context::PROMISE_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(promise_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(1);
    shared->set_length(1);

    InstallSpeciesGetter(isolate_, promise_fun);

    Handle<JSFunction> promise_all = InstallFunctionWithBuiltinId(
        isolate_, promise_fun, "all", Builtins::kPromiseAll, 1, true);
    native_context()->set_promise_all(*promise_all);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "race",
                                 Builtins::kPromiseRace, 1, true);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "resolve",
                                 Builtins::kPromiseResolveTrampoline, 1, true);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "reject",
                                 Builtins::kPromiseReject, 1, true);

    // Setup %PromisePrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(promise_fun->instance_prototype()), isolate());
    native_context()->set_promise_prototype(*prototype);

    InstallToStringTag(isolate_, prototype, factory->Promise_string());

    Handle<JSFunction> promise_then = InstallFunctionWithBuiltinId(
        isolate_, prototype, "then", Builtins::kPromisePrototypeThen, 2, true);
    native_context()->set_promise_then(*promise_then);

    Handle<JSFunction> promise_catch =
        InstallFunctionWithBuiltinId(isolate_, prototype, "catch",
                                     Builtins::kPromisePrototypeCatch, 1, true);
    native_context()->set_promise_catch(*promise_catch);

    InstallFunctionWithBuiltinId(isolate_, prototype, "finally",
                                 Builtins::kPromisePrototypeFinally, 1, true);

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseThenFinally,
          isolate_->factory()->empty_string(), 1);
      info->set_native(true);
      native_context()->set_promise_then_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseCatchFinally,
          isolate_->factory()->empty_string(), 1);
      info->set_native(true);
      native_context()->set_promise_catch_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseValueThunkFinally,
          isolate_->factory()->empty_string(), 0);
      native_context()->set_promise_value_thunk_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseThrowerFinally,
          isolate_->factory()->empty_string(), 0);
      native_context()->set_promise_thrower_finally_shared_fun(*info);
    }

    // Force the Promise constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof Promise
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(promise_fun), 0,
                                "Bootstrapping");

    Handle<Map> prototype_map(prototype->map(), isolate());
    Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);

    {  // Internal: IsPromise
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate_, factory->empty_string(), Builtins::kIsPromise, 1, false);
      native_context()->set_is_promise(*function);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseCapabilityDefaultResolve,
          factory->empty_string(), 1, FunctionKind::kConciseMethod);
      info->set_native(true);
      info->set_function_map_index(
          Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
      native_context()->set_promise_capability_default_resolve_shared_fun(
          *info);

      info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseCapabilityDefaultReject,
          factory->empty_string(), 1, FunctionKind::kConciseMethod);
      info->set_native(true);
      info->set_function_map_index(
          Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
      native_context()->set_promise_capability_default_reject_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseAllResolveElementClosure,
          factory->empty_string(), 1);
      native_context()->set_promise_all_resolve_element_shared_fun(*info);
    }

    // Force the Promise constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof Promise
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(promise_fun, 0, "Bootstrapping");
  }

  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun = InstallFunction(
        isolate_, global, "RegExp", JS_REG_EXP_TYPE,
        JSRegExp::kHeaderSize + JSRegExp::kInObjectFieldCount * kTaggedSize,
        JSRegExp::kInObjectFieldCount, factory->the_hole_value(),
        Builtins::kRegExpConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, regexp_fun,
                                     Context::REGEXP_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(regexp_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(2);
    shared->set_length(2);

    {
      // Setup %RegExpPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(regexp_fun->instance_prototype()), isolate());
      native_context()->set_regexp_prototype(*prototype);

      {
        Handle<JSFunction> fun =
            SimpleInstallFunction(isolate_, prototype, "exec",
                                  Builtins::kRegExpPrototypeExec, 1, true);
        native_context()->set_regexp_exec_function(*fun);
        DCHECK_EQ(JSRegExp::kExecFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      SimpleInstallGetter(isolate_, prototype, factory->dotAll_string(),
                          Builtins::kRegExpPrototypeDotAllGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->flags_string(),
                          Builtins::kRegExpPrototypeFlagsGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->global_string(),
                          Builtins::kRegExpPrototypeGlobalGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->ignoreCase_string(),
                          Builtins::kRegExpPrototypeIgnoreCaseGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->multiline_string(),
                          Builtins::kRegExpPrototypeMultilineGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->source_string(),
                          Builtins::kRegExpPrototypeSourceGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->sticky_string(),
                          Builtins::kRegExpPrototypeStickyGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->unicode_string(),
                          Builtins::kRegExpPrototypeUnicodeGetter, true);

      SimpleInstallFunction(isolate_, prototype, "compile",
                            Builtins::kRegExpPrototypeCompile, 2, true);
      SimpleInstallFunction(isolate_, prototype, "toString",
                            Builtins::kRegExpPrototypeToString, 0, false);
      SimpleInstallFunction(isolate_, prototype, "test",
                            Builtins::kRegExpPrototypeTest, 1, true);

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->match_symbol(), "[Symbol.match]",
            Builtins::kRegExpPrototypeMatch, 1, true);
        native_context()->set_regexp_match_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolMatchFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->match_all_symbol(),
            "[Symbol.matchAll]", Builtins::kRegExpPrototypeMatchAll, 1, true);
        native_context()->set_regexp_match_all_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolMatchAllFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->replace_symbol(), "[Symbol.replace]",
            Builtins::kRegExpPrototypeReplace, 2, false);
        native_context()->set_regexp_replace_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolReplaceFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->search_symbol(), "[Symbol.search]",
            Builtins::kRegExpPrototypeSearch, 1, true);
        native_context()->set_regexp_search_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolSearchFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->split_symbol(), "[Symbol.split]",
            Builtins::kRegExpPrototypeSplit, 2, false);
        native_context()->set_regexp_split_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolSplitFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      Handle<Map> prototype_map(prototype->map(), isolate());
      Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);

      // Store the initial RegExp.prototype map. This is used in fast-path
      // checks. Do not alter the prototype after this point.
      native_context()->set_regexp_prototype_map(*prototype_map);
    }

    {
      // RegExp getters and setters.

      InstallSpeciesGetter(isolate_, regexp_fun);

      // Static properties set by a successful match.

      SimpleInstallGetterSetter(isolate_, regexp_fun, factory->input_string(),
                                Builtins::kRegExpInputGetter,
                                Builtins::kRegExpInputSetter);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$_",
                                Builtins::kRegExpInputGetter,
                                Builtins::kRegExpInputSetter);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "lastMatch",
                                Builtins::kRegExpLastMatchGetter,
                                Builtins::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$&",
                                Builtins::kRegExpLastMatchGetter,
                                Builtins::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "lastParen",
                                Builtins::kRegExpLastParenGetter,
                                Builtins::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$+",
                                Builtins::kRegExpLastParenGetter,
                                Builtins::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "leftContext",
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$`",
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "rightContext",
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$'",
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction);

#define INSTALL_CAPTURE_GETTER(i)                                \
  SimpleInstallGetterSetter(isolate_, regexp_fun, "$" #i,        \
                            Builtins::kRegExpCapture##i##Getter, \
                            Builtins::kEmptyFunction)
      INSTALL_CAPTURE_GETTER(1);
      INSTALL_CAPTURE_GETTER(2);
      INSTALL_CAPTURE_GETTER(3);
      INSTALL_CAPTURE_GETTER(4);
      INSTALL_CAPTURE_GETTER(5);
      INSTALL_CAPTURE_GETTER(6);
      INSTALL_CAPTURE_GETTER(7);
      INSTALL_CAPTURE_GETTER(8);
      INSTALL_CAPTURE_GETTER(9);
#undef INSTALL_CAPTURE_GETTER
    }

    DCHECK(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map(), isolate());

    DCHECK_EQ(1, initial_map->GetInObjectProperties());

    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    // ECMA-262, section 15.10.7.5.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    Descriptor d = Descriptor::DataField(isolate(), factory->lastIndex_string(),
                                         JSRegExp::kLastIndexFieldIndex,
                                         writable, Representation::Tagged());
    initial_map->AppendDescriptor(isolate(), &d);

    // Create the last match info.
    Handle<RegExpMatchInfo> last_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_last_match_info(*last_match_info);

    // Install the species protector cell.
    {
      Handle<PropertyCell> cell =
          factory->NewPropertyCell(factory->empty_string());
      cell->set_value(Smi::FromInt(Protectors::kProtectorValid));
      native_context()->set_regexp_species_protector(*cell);
    }

    // Force the RegExp constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof RegExp
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(regexp_fun, 0, "Bootstrapping");
  }

  {  // --- R e g E x p S t r i n g  I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> regexp_string_iterator_prototype = factory->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(regexp_string_iterator_prototype,
                                iterator_prototype);

    InstallToStringTag(isolate(), regexp_string_iterator_prototype,
                       "RegExp String Iterator");

    SimpleInstallFunction(isolate(), regexp_string_iterator_prototype, "next",
                          Builtins::kRegExpStringIteratorPrototypeNext, 0,
                          true);

    Handle<JSFunction> regexp_string_iterator_function = CreateFunction(
        isolate(), "RegExpStringIterator", JS_REG_EXP_STRING_ITERATOR_TYPE,
        JSRegExpStringIterator::kHeaderSize, 0,
        regexp_string_iterator_prototype, Builtins::kIllegal);
    regexp_string_iterator_function->shared().set_native(false);
    native_context()->set_initial_regexp_string_iterator_prototype_map(
        regexp_string_iterator_function->initial_map());
  }

  {  // -- E r r o r
    InstallError(isolate_, global, factory->Error_string(),
                 Context::ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeError, Context::MAKE_ERROR_INDEX);
  }

  {  // -- E v a l E r r o r
    InstallError(isolate_, global, factory->EvalError_string(),
                 Context::EVAL_ERROR_FUNCTION_INDEX);
  }

  {  // -- R a n g e E r r o r
    InstallError(isolate_, global, factory->RangeError_string(),
                 Context::RANGE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeRangeError,
                     Context::MAKE_RANGE_ERROR_INDEX);
  }

  {  // -- R e f e r e n c e E r r o r
    InstallError(isolate_, global, factory->ReferenceError_string(),
                 Context::REFERENCE_ERROR_FUNCTION_INDEX);
  }

  {  // -- S y n t a x E r r o r
    InstallError(isolate_, global, factory->SyntaxError_string(),
                 Context::SYNTAX_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeSyntaxError,
                     Context::MAKE_SYNTAX_ERROR_INDEX);
  }

  {  // -- T y p e E r r o r
    InstallError(isolate_, global, factory->TypeError_string(),
                 Context::TYPE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeTypeError,
                     Context::MAKE_TYPE_ERROR_INDEX);
  }

  {  // -- U R I E r r o r
    InstallError(isolate_, global, factory->URIError_string(),
                 Context::URI_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeURIError,
                     Context::MAKE_URI_ERROR_INDEX);
  }

  {  // -- C o m p i l e E r r o r
    Handle<JSObject> dummy = factory->NewJSObject(isolate_->object_function());
    InstallError(isolate_, dummy, factory->CompileError_string(),
                 Context::WASM_COMPILE_ERROR_FUNCTION_INDEX);

    // -- L i n k E r r o r
    InstallError(isolate_, dummy, factory->LinkError_string(),
                 Context::WASM_LINK_ERROR_FUNCTION_INDEX);

    // -- R u n t i m e E r r o r
    InstallError(isolate_, dummy, factory->RuntimeError_string(),
                 Context::WASM_RUNTIME_ERROR_FUNCTION_INDEX);
  }

  // Initialize the embedder data slot.
  // TODO(ishell): microtask queue pointer will be moved from native context
  // to the embedder data array so we don't need an empty embedder data array.
  Handle<EmbedderDataArray> embedder_data = factory->NewEmbedderDataArray(0);
  native_context()->set_embedder_data(*embedder_data);

  {  // -- g l o b a l T h i s
    Handle<JSGlobalProxy> global_proxy(native_context()->global_proxy(),
                                       isolate_);
    JSObject::AddProperty(isolate_, global, factory->globalThis_string(),
                          global_proxy, DONT_ENUM);
  }

  {  // -- J S O N
    Handle<JSObject> json_object =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "JSON", json_object, DONT_ENUM);
    SimpleInstallFunction(isolate_, json_object, "parse", Builtins::kJsonParse,
                          2, false);
    SimpleInstallFunction(isolate_, json_object, "stringify",
                          Builtins::kJsonStringify, 3, true);
    InstallToStringTag(isolate_, json_object, "JSON");
  }

  {  // -- M a t h
    Handle<JSObject> math =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "Math", math, DONT_ENUM);
    SimpleInstallFunction(isolate_, math, "abs", Builtins::kMathAbs, 1, true);
    SimpleInstallFunction(isolate_, math, "acos", Builtins::kMathAcos, 1, true);
    SimpleInstallFunction(isolate_, math, "acosh", Builtins::kMathAcosh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "asin", Builtins::kMathAsin, 1, true);
    SimpleInstallFunction(isolate_, math, "asinh", Builtins::kMathAsinh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan", Builtins::kMathAtan, 1, true);
    SimpleInstallFunction(isolate_, math, "atanh", Builtins::kMathAtanh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan2", Builtins::kMathAtan2, 2,
                          true);
    SimpleInstallFunction(isolate_, math, "ceil", Builtins::kMathCeil, 1, true);
    SimpleInstallFunction(isolate_, math, "cbrt", Builtins::kMathCbrt, 1, true);
    SimpleInstallFunction(isolate_, math, "expm1", Builtins::kMathExpm1, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "clz32", Builtins::kMathClz32, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "cos", Builtins::kMathCos, 1, true);
    SimpleInstallFunction(isolate_, math, "cosh", Builtins::kMathCosh, 1, true);
    SimpleInstallFunction(isolate_, math, "exp", Builtins::kMathExp, 1, true);
    Handle<JSFunction> math_floor = SimpleInstallFunction(
        isolate_, math, "floor", Builtins::kMathFloor, 1, true);
    native_context()->set_math_floor(*math_floor);
    SimpleInstallFunction(isolate_, math, "fround", Builtins::kMathFround, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "hypot", Builtins::kMathHypot, 2,
                          false);
    SimpleInstallFunction(isolate_, math, "imul", Builtins::kMathImul, 2, true);
    SimpleInstallFunction(isolate_, math, "log", Builtins::kMathLog, 1, true);
    SimpleInstallFunction(isolate_, math, "log1p", Builtins::kMathLog1p, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "log2", Builtins::kMathLog2, 1, true);
    SimpleInstallFunction(isolate_, math, "log10", Builtins::kMathLog10, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "max", Builtins::kMathMax, 2, false);
    SimpleInstallFunction(isolate_, math, "min", Builtins::kMathMin, 2, false);
    Handle<JSFunction> math_pow = SimpleInstallFunction(
        isolate_, math, "pow", Builtins::kMathPow, 2, true);
    native_context()->set_math_pow(*math_pow);
    SimpleInstallFunction(isolate_, math, "random", Builtins::kMathRandom, 0,
                          true);
    SimpleInstallFunction(isolate_, math, "round", Builtins::kMathRound, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "sign", Builtins::kMathSign, 1, true);
    SimpleInstallFunction(isolate_, math, "sin", Builtins::kMathSin, 1, true);
    SimpleInstallFunction(isolate_, math, "sinh", Builtins::kMathSinh, 1, true);
    SimpleInstallFunction(isolate_, math, "sqrt", Builtins::kMathSqrt, 1, true);
    SimpleInstallFunction(isolate_, math, "tan", Builtins::kMathTan, 1, true);
    SimpleInstallFunction(isolate_, math, "tanh", Builtins::kMathTanh, 1, true);
    SimpleInstallFunction(isolate_, math, "trunc", Builtins::kMathTrunc, 1,
                          true);

    // Install math constants.
    double const kE = base::ieee754::exp(1.0);
    double const kPI = 3.1415926535897932;
    InstallConstant(isolate_, math, "E", factory->NewNumber(kE));
    InstallConstant(isolate_, math, "LN10",
                    factory->NewNumber(base::ieee754::log(10.0)));
    InstallConstant(isolate_, math, "LN2",
                    factory->NewNumber(base::ieee754::log(2.0)));
    InstallConstant(isolate_, math, "LOG10E",
                    factory->NewNumber(base::ieee754::log10(kE)));
    InstallConstant(isolate_, math, "LOG2E",
                    factory->NewNumber(base::ieee754::log2(kE)));
    InstallConstant(isolate_, math, "PI", factory->NewNumber(kPI));
    InstallConstant(isolate_, math, "SQRT1_2",
                    factory->NewNumber(std::sqrt(0.5)));
    InstallConstant(isolate_, math, "SQRT2",
                    factory->NewNumber(std::sqrt(2.0)));
    InstallToStringTag(isolate_, math, "Math");
  }

  {  // -- C o n s o l e
    Handle<String> name = factory->InternalizeUtf8String("console");
    NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(
        name, isolate_->strict_function_map(), LanguageMode::kStrict);
    Handle<JSFunction> cons = factory->NewFunction(args);

    Handle<JSObject> empty = factory->NewJSObject(isolate_->object_function());
    JSFunction::SetPrototype(cons, empty);

    Handle<JSObject> console = factory->NewJSObject(cons, AllocationType::kOld);
    DCHECK(console->IsJSObject());
    JSObject::AddProperty(isolate_, global, name, console, DONT_ENUM);
    SimpleInstallFunction(isolate_, console, "debug", Builtins::kConsoleDebug,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "error", Builtins::kConsoleError,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "info", Builtins::kConsoleInfo, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "log", Builtins::kConsoleLog, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "warn", Builtins::kConsoleWarn, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dir", Builtins::kConsoleDir, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dirxml", Builtins::kConsoleDirXml,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "table", Builtins::kConsoleTable,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "trace", Builtins::kConsoleTrace,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "group", Builtins::kConsoleGroup,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "groupCollapsed",
                          Builtins::kConsoleGroupCollapsed, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "groupEnd",
                          Builtins::kConsoleGroupEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "clear", Builtins::kConsoleClear,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "count", Builtins::kConsoleCount,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "countReset",
                          Builtins::kConsoleCountReset, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "assert",
                          Builtins::kFastConsoleAssert, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "profile",
                          Builtins::kConsoleProfile, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "profileEnd",
                          Builtins::kConsoleProfileEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "time", Builtins::kConsoleTime, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "timeLog",
                          Builtins::kConsoleTimeLog, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeEnd",
                          Builtins::kConsoleTimeEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeStamp",
                          Builtins::kConsoleTimeStamp, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "context",
                          Builtins::kConsoleContext, 1, true, NONE);
    InstallToStringTag(isolate_, console, "Object");
  }

#ifdef V8_INTL_SUPPORT
  {  // -- I n t l
    Handle<JSObject> intl =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "Intl", intl, DONT_ENUM);

    SimpleInstallFunction(isolate(), intl, "getCanonicalLocales",
                          Builtins::kIntlGetCanonicalLocales, 1, false);

    {  // -- D a t e T i m e F o r m a t
      Handle<JSFunction> date_time_format_constructor = InstallFunction(
          isolate_, intl, "DateTimeFormat", JS_DATE_TIME_FORMAT_TYPE,
          JSDateTimeFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kDateTimeFormatConstructor);
      date_time_format_constructor->shared().set_length(0);
      date_time_format_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, date_time_format_constructor,
          Context::INTL_DATE_TIME_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), date_time_format_constructor, "supportedLocalesOf",
          Builtins::kDateTimeFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(date_time_format_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kDateTimeFormatPrototypeResolvedOptions,
                            0, false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtins::kDateTimeFormatPrototypeFormatToParts, 1,
                            false);

      SimpleInstallGetter(isolate_, prototype, factory->format_string(),
                          Builtins::kDateTimeFormatPrototypeFormat, false);

      SimpleInstallFunction(isolate_, prototype, "formatRange",
                            Builtins::kDateTimeFormatPrototypeFormatRange, 2,
                            false);
      SimpleInstallFunction(
          isolate_, prototype, "formatRangeToParts",
          Builtins::kDateTimeFormatPrototypeFormatRangeToParts, 2, false);
    }

    {  // -- N u m b e r F o r m a t
      Handle<JSFunction> number_format_constructor = InstallFunction(
          isolate_, intl, "NumberFormat", JS_NUMBER_FORMAT_TYPE,
          JSNumberFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kNumberFormatConstructor);
      number_format_constructor->shared().set_length(0);
      number_format_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, number_format_constructor,
          Context::INTL_NUMBER_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), number_format_constructor, "supportedLocalesOf",
          Builtins::kNumberFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(number_format_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kNumberFormatPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtins::kNumberFormatPrototypeFormatToParts, 1,
                            false);
      SimpleInstallGetter(isolate_, prototype, factory->format_string(),
                          Builtins::kNumberFormatPrototypeFormatNumber, false);
    }

    {  // -- C o l l a t o r
      Handle<JSFunction> collator_constructor = InstallFunction(
          isolate_, intl, "Collator", JS_COLLATOR_TYPE, JSCollator::kHeaderSize,
          0, factory->the_hole_value(), Builtins::kCollatorConstructor);
      collator_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(isolate_, collator_constructor,
                                       Context::INTL_COLLATOR_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), collator_constructor,
                            "supportedLocalesOf",
                            Builtins::kCollatorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(collator_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kCollatorPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallGetter(isolate_, prototype, factory->compare_string(),
                          Builtins::kCollatorPrototypeCompare, false);
    }

    {  // -- V 8 B r e a k I t e r a t o r
      Handle<JSFunction> v8_break_iterator_constructor = InstallFunction(
          isolate_, intl, "v8BreakIterator", JS_V8_BREAK_ITERATOR_TYPE,
          JSV8BreakIterator::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kV8BreakIteratorConstructor);
      v8_break_iterator_constructor->shared().DontAdaptArguments();

      SimpleInstallFunction(
          isolate_, v8_break_iterator_constructor, "supportedLocalesOf",
          Builtins::kV8BreakIteratorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(v8_break_iterator_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kV8BreakIteratorPrototypeResolvedOptions,
                            0, false);

      SimpleInstallGetter(isolate_, prototype, factory->adoptText_string(),
                          Builtins::kV8BreakIteratorPrototypeAdoptText, false);

      SimpleInstallGetter(isolate_, prototype, factory->first_string(),
                          Builtins::kV8BreakIteratorPrototypeFirst, false);

      SimpleInstallGetter(isolate_, prototype, factory->next_string(),
                          Builtins::kV8BreakIteratorPrototypeNext, false);

      SimpleInstallGetter(isolate_, prototype, factory->current_string(),
                          Builtins::kV8BreakIteratorPrototypeCurrent, false);

      SimpleInstallGetter(isolate_, prototype, factory->breakType_string(),
                          Builtins::kV8BreakIteratorPrototypeBreakType, false);
    }

    {  // -- P l u r a l R u l e s
      Handle<JSFunction> plural_rules_constructor = InstallFunction(
          isolate_, intl, "PluralRules", JS_PLURAL_RULES_TYPE,
          JSPluralRules::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kPluralRulesConstructor);
      plural_rules_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, plural_rules_constructor,
          Context::INTL_PLURAL_RULES_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), plural_rules_constructor,
                            "supportedLocalesOf",
                            Builtins::kPluralRulesSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(plural_rules_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kPluralRulesPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "select",
                            Builtins::kPluralRulesPrototypeSelect, 1, false);
    }

    {  // -- R e l a t i v e T i m e F o r m a t
      Handle<JSFunction> relative_time_format_fun = InstallFunction(
          isolate(), intl, "RelativeTimeFormat", JS_RELATIVE_TIME_FORMAT_TYPE,
          JSRelativeTimeFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kRelativeTimeFormatConstructor);
      relative_time_format_fun->shared().set_length(0);
      relative_time_format_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, relative_time_format_fun,
          Context::INTL_RELATIVE_TIME_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), relative_time_format_fun, "supportedLocalesOf",
          Builtins::kRelativeTimeFormatSupportedLocalesOf, 1, false);

      // Setup %RelativeTimeFormatPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(relative_time_format_fun->instance_prototype()),
          isolate());

      InstallToStringTag(isolate(), prototype, "Intl.RelativeTimeFormat");

      SimpleInstallFunction(
          isolate(), prototype, "resolvedOptions",
          Builtins::kRelativeTimeFormatPrototypeResolvedOptions, 0, false);
      SimpleInstallFunction(isolate(), prototype, "format",
                            Builtins::kRelativeTimeFormatPrototypeFormat, 2,
                            false);
      SimpleInstallFunction(isolate(), prototype, "formatToParts",
                            Builtins::kRelativeTimeFormatPrototypeFormatToParts,
                            2, false);
    }

    {  // -- L i s t F o r m a t
      Handle<JSFunction> list_format_fun = InstallFunction(
          isolate(), intl, "ListFormat", JS_LIST_FORMAT_TYPE,
          JSListFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtins::kListFormatConstructor);
      list_format_fun->shared().set_length(0);
      list_format_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, list_format_fun, Context::INTL_LIST_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), list_format_fun, "supportedLocalesOf",
                            Builtins::kListFormatSupportedLocalesOf, 1, false);

      // Setup %ListFormatPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(list_format_fun->instance_prototype()), isolate());

      InstallToStringTag(isolate(), prototype, "Intl.ListFormat");

      SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                            Builtins::kListFormatPrototypeResolvedOptions, 0,
                            false);
      SimpleInstallFunction(isolate(), prototype, "format",
                            Builtins::kListFormatPrototypeFormat, 1, false);
      SimpleInstallFunction(isolate(), prototype, "formatToParts",
                            Builtins::kListFormatPrototypeFormatToParts, 1,
                            false);
    }

    {  // -- L o c a l e
      Handle<JSFunction> locale_fun = InstallFunction(
          isolate(), intl, "Locale", JS_LOCALE_TYPE, JSLocale::kHeaderSize, 0,
          factory->the_hole_value(), Builtins::kLocaleConstructor);
      InstallWithIntrinsicDefaultProto(isolate(), locale_fun,
                                       Context::INTL_LOCALE_FUNCTION_INDEX);
      locale_fun->shared().set_length(1);
      locale_fun->shared().DontAdaptArguments();

      // Setup %LocalePrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(locale_fun->instance_prototype()), isolate());

      InstallToStringTag(isolate(), prototype, "Intl.Locale");

      SimpleInstallFunction(isolate(), prototype, "toString",
                            Builtins::kLocalePrototypeToString, 0, false);
      SimpleInstallFunction(isolate(), prototype, "maximize",
                            Builtins::kLocalePrototypeMaximize, 0, false);
      SimpleInstallFunction(isolate(), prototype, "minimize",
                            Builtins::kLocalePrototypeMinimize, 0, false);
      // Base locale getters.
      SimpleInstallGetter(isolate(), prototype, factory->language_string(),
                          Builtins::kLocalePrototypeLanguage, true);
      SimpleInstallGetter(isolate(), prototype, factory->script_string(),
                          Builtins::kLocalePrototypeScript, true);
      SimpleInstallGetter(isolate(), prototype, factory->region_string(),
                          Builtins::kLocalePrototypeRegion, true);
      SimpleInstallGetter(isolate(), prototype, factory->baseName_string(),
                          Builtins::kLocalePrototypeBaseName, true);
      // Unicode extension getters.
      SimpleInstallGetter(isolate(), prototype, factory->calendar_string(),
                          Builtins::kLocalePrototypeCalendar, true);
      SimpleInstallGetter(isolate(), prototype, factory->caseFirst_string(),
                          Builtins::kLocalePrototypeCaseFirst, true);
      SimpleInstallGetter(isolate(), prototype, factory->collation_string(),
                          Builtins::kLocalePrototypeCollation, true);
      SimpleInstallGetter(isolate(), prototype, factory->hourCycle_string(),
                          Builtins::kLocalePrototypeHourCycle, true);
      SimpleInstallGetter(isolate(), prototype, factory->numeric_string(),
                          Builtins::kLocalePrototypeNumeric, true);
      SimpleInstallGetter(isolate(), prototype,
                          factory->numberingSystem_string(),
                          Builtins::kLocalePrototypeNumberingSystem, true);
    }
  }
#endif  // V8_INTL_SUPPORT

  {  // -- A r r a y B u f f e r
    Handle<String> name = factory->ArrayBuffer_string();
    Handle<JSFunction> array_buffer_fun = CreateArrayBuffer(name, ARRAY_BUFFER);
    JSObject::AddProperty(isolate_, global, name, array_buffer_fun, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(isolate_, array_buffer_fun,
                                     Context::ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, array_buffer_fun);

    Handle<JSFunction> array_buffer_noinit_fun = SimpleCreateFunction(
        isolate_,
        factory->InternalizeUtf8String(
            "arrayBufferConstructor_DoNotInitialize"),
        Builtins::kArrayBufferConstructor_DoNotInitialize, 1, false);
    native_context()->set_array_buffer_noinit_fun(*array_buffer_noinit_fun);
  }

  {  // -- S h a r e d A r r a y B u f f e r
    Handle<String> name = factory->SharedArrayBuffer_string();
    Handle<JSFunction> shared_array_buffer_fun =
        CreateArrayBuffer(name, SHARED_ARRAY_BUFFER);
    InstallWithIntrinsicDefaultProto(isolate_, shared_array_buffer_fun,
                                     Context::SHARED_ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, shared_array_buffer_fun);
  }

  {  // -- A t o m i c s
    Handle<JSObject> atomics_object =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    native_context()->set_atomics_object(*atomics_object);

    SimpleInstallFunction(isolate_, atomics_object, "load",
                          Builtins::kAtomicsLoad, 2, true);
    SimpleInstallFunction(isolate_, atomics_object, "store",
                          Builtins::kAtomicsStore, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "add",
                          Builtins::kAtomicsAdd, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "sub",
                          Builtins::kAtomicsSub, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "and",
                          Builtins::kAtomicsAnd, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "or", Builtins::kAtomicsOr,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "xor",
                          Builtins::kAtomicsXor, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "exchange",
                          Builtins::kAtomicsExchange, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "compareExchange",
                          Builtins::kAtomicsCompareExchange, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "isLockFree",
                          Builtins::kAtomicsIsLockFree, 1, true);
    SimpleInstallFunction(isolate_, atomics_object, "wait",
                          Builtins::kAtomicsWait, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "wake",
                          Builtins::kAtomicsWake, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "notify",
                          Builtins::kAtomicsNotify, 3, true);
  }

  {  // -- T y p e d A r r a y
    Handle<JSFunction> typed_array_fun = CreateFunction(
        isolate_, factory->InternalizeUtf8String("TypedArray"),
        JS_TYPED_ARRAY_TYPE, JSTypedArray::kHeaderSize, 0,
        factory->the_hole_value(), Builtins::kTypedArrayBaseConstructor);
    typed_array_fun->shared().set_native(false);
    typed_array_fun->shared().set_length(0);
    InstallSpeciesGetter(isolate_, typed_array_fun);
    native_context()->set_typed_array_function(*typed_array_fun);

    SimpleInstallFunction(isolate_, typed_array_fun, "of",
                          Builtins::kTypedArrayOf, 0, false);
    SimpleInstallFunction(isolate_, typed_array_fun, "from",
                          Builtins::kTypedArrayFrom, 1, false);

    // Setup %TypedArrayPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(typed_array_fun->instance_prototype()), isolate());
    native_context()->set_typed_array_prototype(*prototype);

    // Install the "buffer", "byteOffset", "byteLength", "length"
    // and @@toStringTag getters on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtins::kTypedArrayPrototypeBuffer, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtins::kTypedArrayPrototypeByteLength, true);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtins::kTypedArrayPrototypeByteOffset, true);
    SimpleInstallGetter(isolate_, prototype, factory->length_string(),
                        Builtins::kTypedArrayPrototypeLength, true);
    SimpleInstallGetter(isolate_, prototype, factory->to_string_tag_symbol(),
                        Builtins::kTypedArrayPrototypeToStringTag, true);

    // Install "keys", "values" and "entries" methods on the {prototype}.
    InstallFunctionWithBuiltinId(isolate_, prototype, "entries",
                                 Builtins::kTypedArrayPrototypeEntries, 0,
                                 true);

    InstallFunctionWithBuiltinId(isolate_, prototype, "keys",
                                 Builtins::kTypedArrayPrototypeKeys, 0, true);

    Handle<JSFunction> values = InstallFunctionWithBuiltinId(
        isolate_, prototype, "values", Builtins::kTypedArrayPrototypeValues, 0,
        true);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    // TODO(caitp): alphasort accessors/methods
    SimpleInstallFunction(isolate_, prototype, "copyWithin",
                          Builtins::kTypedArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, prototype, "every",
                          Builtins::kTypedArrayPrototypeEvery, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fill",
                          Builtins::kTypedArrayPrototypeFill, 1, false);
    SimpleInstallFunction(isolate_, prototype, "filter",
                          Builtins::kTypedArrayPrototypeFilter, 1, false);
    SimpleInstallFunction(isolate_, prototype, "find",
                          Builtins::kTypedArrayPrototypeFind, 1, false);
    SimpleInstallFunction(isolate_, prototype, "findIndex",
                          Builtins::kTypedArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kTypedArrayPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtins::kTypedArrayPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtins::kTypedArrayPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "join",
                          Builtins::kTypedArrayPrototypeJoin, 1, false);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtins::kTypedArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "map",
                          Builtins::kTypedArrayPrototypeMap, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reverse",
                          Builtins::kTypedArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, prototype, "reduce",
                          Builtins::kTypedArrayPrototypeReduce, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reduceRight",
                          Builtins::kTypedArrayPrototypeReduceRight, 1, false);
    SimpleInstallFunction(isolate_, prototype, "set",
                          Builtins::kTypedArrayPrototypeSet, 1, false);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtins::kTypedArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "some",
                          Builtins::kTypedArrayPrototypeSome, 1, false);
    SimpleInstallFunction(isolate_, prototype, "sort",
                          Builtins::kTypedArrayPrototypeSort, 1, false);
    SimpleInstallFunction(isolate_, prototype, "subarray",
                          Builtins::kTypedArrayPrototypeSubArray, 2, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kTypedArrayPrototypeToLocaleString, 0,
                          false);
    JSObject::AddProperty(isolate_, prototype, factory->toString_string(),
                          array_prototype_to_string_fun, DONT_ENUM);
  }

  {// -- T y p e d A r r a y s
#define INSTALL_TYPED_ARRAY(Type, type, TYPE, ctype)                   \
  {                                                                    \
    Handle<JSFunction> fun =                                           \
        InstallTypedArray(#Type "Array", TYPE##_ELEMENTS);             \
    InstallWithIntrinsicDefaultProto(isolate_, fun,                    \
                                     Context::TYPE##_ARRAY_FUN_INDEX); \
  }
  TYPED_ARRAYS(INSTALL_TYPED_ARRAY)
#undef INSTALL_TYPED_ARRAY
  }

  {  // -- D a t a V i e w
    Handle<JSFunction> data_view_fun = InstallFunction(
        isolate_, global, "DataView", JS_DATA_VIEW_TYPE,
        JSDataView::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtins::kDataViewConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, data_view_fun,
                                     Context::DATA_VIEW_FUN_INDEX);
    data_view_fun->shared().set_length(1);
    data_view_fun->shared().DontAdaptArguments();

    // Setup %DataViewPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(data_view_fun->instance_prototype()), isolate());

    InstallToStringTag(isolate_, prototype, "DataView");

    // Install the "buffer", "byteOffset" and "byteLength" getters
    // on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtins::kDataViewPrototypeGetBuffer, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtins::kDataViewPrototypeGetByteLength, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtins::kDataViewPrototypeGetByteOffset, false);

    SimpleInstallFunction(isolate_, prototype, "getInt8",
                          Builtins::kDataViewPrototypeGetInt8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt8",
                          Builtins::kDataViewPrototypeSetInt8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint8",
                          Builtins::kDataViewPrototypeGetUint8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint8",
                          Builtins::kDataViewPrototypeSetUint8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt16",
                          Builtins::kDataViewPrototypeGetInt16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt16",
                          Builtins::kDataViewPrototypeSetInt16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint16",
                          Builtins::kDataViewPrototypeGetUint16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint16",
                          Builtins::kDataViewPrototypeSetUint16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt32",
                          Builtins::kDataViewPrototypeGetInt32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt32",
                          Builtins::kDataViewPrototypeSetInt32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint32",
                          Builtins::kDataViewPrototypeGetUint32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint32",
                          Builtins::kDataViewPrototypeSetUint32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat32",
                          Builtins::kDataViewPrototypeGetFloat32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat32",
                          Builtins::kDataViewPrototypeSetFloat32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat64",
                          Builtins::kDataViewPrototypeGetFloat64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat64",
                          Builtins::kDataViewPrototypeSetFloat64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigInt64",
                          Builtins::kDataViewPrototypeGetBigInt64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigInt64",
                          Builtins::kDataViewPrototypeSetBigInt64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigUint64",
                          Builtins::kDataViewPrototypeGetBigUint64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigUint64",
                          Builtins::kDataViewPrototypeSetBigUint64, 2, false);
  }

  {  // -- M a p
    Handle<JSFunction> js_map_fun = InstallFunction(
        isolate_, global, "Map", JS_MAP_TYPE, JSMap::kHeaderSize, 0,
        factory->the_hole_value(), Builtins::kMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_map_fun,
                                     Context::JS_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_map_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %MapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_map_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, factory->Map_string());

    Handle<JSFunction> map_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtins::kMapPrototypeGet, 1, true);
    native_context()->set_map_get(*map_get);

    Handle<JSFunction> map_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtins::kMapPrototypeSet, 2, true);
    // Check that index of "set" function in JSCollection is correct.
    DCHECK_EQ(JSCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());
    native_context()->set_map_set(*map_set);

    Handle<JSFunction> map_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtins::kMapPrototypeHas, 1, true);
    native_context()->set_map_has(*map_has);

    Handle<JSFunction> map_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtins::kMapPrototypeDelete, 1, true);
    native_context()->set_map_delete(*map_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtins::kMapPrototypeClear, 0, true);
    Handle<JSFunction> entries =
        SimpleInstallFunction(isolate_, prototype, "entries",
                              Builtins::kMapPrototypeEntries, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          entries, DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kMapPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "keys",
                          Builtins::kMapPrototypeKeys, 0, true);
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("size"),
                        Builtins::kMapPrototypeGetSize, true);
    SimpleInstallFunction(isolate_, prototype, "values",
                          Builtins::kMapPrototypeValues, 0, true);

    native_context()->set_initial_map_prototype_map(prototype->map());

    InstallSpeciesGetter(isolate_, js_map_fun);
  }

  {  // -- B i g I n t
    Handle<JSFunction> bigint_fun = InstallFunction(
        isolate_, global, "BigInt", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0, factory->the_hole_value(),
        Builtins::kBigIntConstructor);
    bigint_fun->shared().DontAdaptArguments();
    bigint_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, bigint_fun,
                                     Context::BIGINT_FUNCTION_INDEX);

    // Install the properties of the BigInt constructor.
    // asUintN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asUintN",
                          Builtins::kBigIntAsUintN, 2, false);
    // asIntN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asIntN",
                          Builtins::kBigIntAsIntN, 2, false);

    // Set up the %BigIntPrototype%.
    Handle<JSObject> prototype(JSObject::cast(bigint_fun->instance_prototype()),
                               isolate_);
    JSFunction::SetPrototype(bigint_fun, prototype);

    // Install the properties of the BigInt.prototype.
    // "constructor" is created implicitly by InstallFunction() above.
    // toLocaleString([reserved1 [, reserved2]])
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kBigIntPrototypeToLocaleString, 0, false);
    // toString([radix])
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kBigIntPrototypeToString, 0, false);
    // valueOf()
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kBigIntPrototypeValueOf, 0, false);
    // @@toStringTag
    InstallToStringTag(isolate_, prototype, factory->BigInt_string());
  }

  {  // -- S e t
    Handle<JSFunction> js_set_fun = InstallFunction(
        isolate_, global, "Set", JS_SET_TYPE, JSSet::kHeaderSize, 0,
        factory->the_hole_value(), Builtins::kSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_set_fun,
                                     Context::JS_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_set_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %SetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_set_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, factory->Set_string());

    Handle<JSFunction> set_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtins::kSetPrototypeHas, 1, true);
    native_context()->set_set_has(*set_has);

    Handle<JSFunction> set_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtins::kSetPrototypeAdd, 1, true);
    // Check that index of "add" function in JSCollection is correct.
    DCHECK_EQ(JSCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());
    native_context()->set_set_add(*set_add);

    Handle<JSFunction> set_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtins::kSetPrototypeDelete, 1, true);
    native_context()->set_set_delete(*set_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtins::kSetPrototypeClear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "entries",
                          Builtins::kSetPrototypeEntries, 0, true);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kSetPrototypeForEach, 1, false);
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("size"),
                        Builtins::kSetPrototypeGetSize, true);
    Handle<JSFunction> values = SimpleInstallFunction(
        isolate_, prototype, "values", Builtins::kSetPrototypeValues, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->keys_string(), values,
                          DONT_ENUM);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    native_context()->set_initial_set_prototype_map(prototype->map());
    native_context()->set_initial_set_prototype(*prototype);

    InstallSpeciesGetter(isolate_, js_set_fun);
  }

  {  // -- J S M o d u l e N a m e s p a c e
    Handle<Map> map = factory->NewMap(
        JS_MODULE_NAMESPACE_TYPE, JSModuleNamespace::kSize,
        TERMINAL_FAST_ELEMENTS_KIND, JSModuleNamespace::kInObjectFieldCount);
    map->SetConstructor(native_context()->object_function());
    Map::SetPrototype(isolate(), map, isolate_->factory()->null_value());
    Map::EnsureDescriptorSlack(isolate_, map, 1);
    native_context()->set_js_module_namespace_map(*map);

    {  // Install @@toStringTag.
      PropertyAttributes attribs =
          static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
      Descriptor d =
          Descriptor::DataField(isolate(), factory->to_string_tag_symbol(),
                                JSModuleNamespace::kToStringTagFieldIndex,
                                attribs, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
  }

  {  // -- I t e r a t o r R e s u l t
    // Setup the map for IterResultObjects created from builtins in such a
    // way that it's exactly the same map as the one produced by object
    // literals in the form `{value, done}`. This way we have better sharing
    // of maps (i.e. less polymorphism) and also make it possible to hit the
    // fast-paths in various builtins (i.e. promises and collections) with
    // user defined iterators.
    Handle<Map> map = factory->ObjectLiteralMapFromCache(native_context(), 2);

    // value
    map = Map::CopyWithField(isolate(), map, factory->value_string(),
                             FieldType::Any(isolate()), NONE,
                             PropertyConstness::kConst,
                             Representation::Tagged(), INSERT_TRANSITION)
              .ToHandleChecked();

    // done
    // TODO(bmeurer): Once FLAG_modify_field_representation_inplace is always
    // on, we can say Representation::HeapObject() here and have the inplace
    // update logic take care of the case where someone ever stores a Smi into
    // the done field.
    map = Map::CopyWithField(isolate(), map, factory->done_string(),
                             FieldType::Any(isolate()), NONE,
                             PropertyConstness::kConst,
                             Representation::Tagged(), INSERT_TRANSITION)
              .ToHandleChecked();

    native_context()->set_iterator_result_map(*map);
  }

  {  // -- W e a k M a p
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kHeaderSize,
        0, factory->the_hole_value(), Builtins::kWeakMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakMapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    SimpleInstallFunction(isolate_, prototype, "delete",
                          Builtins::kWeakMapPrototypeDelete, 1, true);
    Handle<JSFunction> weakmap_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtins::kWeakMapGet, 1, true);
    native_context()->set_weakmap_get(*weakmap_get);

    Handle<JSFunction> weakmap_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtins::kWeakMapPrototypeSet, 2, true);
    // Check that index of "set" function in JSWeakCollection is correct.
    DCHECK_EQ(JSWeakCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());

    native_context()->set_weakmap_set(*weakmap_set);
    SimpleInstallFunction(isolate_, prototype, "has",
                          Builtins::kWeakMapPrototypeHas, 1, true);

    InstallToStringTag(isolate_, prototype, "WeakMap");

    native_context()->set_initial_weakmap_prototype_map(prototype->map());
  }

  {  // -- W e a k S e t
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kHeaderSize,
        0, factory->the_hole_value(), Builtins::kWeakSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakSetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    SimpleInstallFunction(isolate_, prototype, "delete",
                          Builtins::kWeakSetPrototypeDelete, 1, true);
    SimpleInstallFunction(isolate_, prototype, "has",
                          Builtins::kWeakSetPrototypeHas, 1, true);

    Handle<JSFunction> weakset_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtins::kWeakSetPrototypeAdd, 1, true);
    // Check that index of "add" function in JSWeakCollection is correct.
    DCHECK_EQ(JSWeakCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());

    native_context()->set_weakset_add(*weakset_add);

    InstallToStringTag(isolate_, prototype,
                       factory->InternalizeUtf8String("WeakSet"));

    native_context()->set_initial_weakset_prototype_map(prototype->map());
  }

  {  // -- P r o x y
    CreateJSProxyMaps();
    // Proxy function map has prototype slot for storing initial map but does
    // not have a prototype property.
    Handle<Map> proxy_function_map = Map::Copy(
        isolate_, isolate_->strict_function_without_prototype_map(), "Proxy");
    proxy_function_map->set_is_constructor(true);

    Handle<String> name = factory->Proxy_string();

    NewFunctionArgs args = NewFunctionArgs::ForBuiltin(
        name, proxy_function_map, Builtins::kProxyConstructor);
    Handle<JSFunction> proxy_function = factory->NewFunction(args);

    isolate_->proxy_map()->SetConstructor(*proxy_function);

    proxy_function->shared().set_internal_formal_parameter_count(2);
    proxy_function->shared().set_length(2);

    native_context()->set_proxy_function(*proxy_function);
    JSObject::AddProperty(isolate_, global, name, proxy_function, DONT_ENUM);

    DCHECK(!proxy_function->has_prototype_property());

    SimpleInstallFunction(isolate_, proxy_function, "revocable",
                          Builtins::kProxyRevocable, 2, true);

    {  // Internal: ProxyRevoke
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kProxyRevoke, factory->empty_string(), 0);
      native_context()->set_proxy_revoke_shared_fun(*info);
    }
  }

  {  // -- R e f l e c t
    Handle<String> reflect_string = factory->InternalizeUtf8String("Reflect");
    Handle<JSObject> reflect =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, reflect_string, reflect, DONT_ENUM);

        SimpleInstallFunction(isolate_, reflect, "defineProperty",
                              Builtins::kReflectDefineProperty, 3, true);

        SimpleInstallFunction(isolate_, reflect, "deleteProperty",
                              Builtins::kReflectDeleteProperty, 2, true);

    Handle<JSFunction> apply = SimpleInstallFunction(
        isolate_, reflect, "apply", Builtins::kReflectApply, 3, false);
    native_context()->set_reflect_apply(*apply);

    Handle<JSFunction> construct = SimpleInstallFunction(
        isolate_, reflect, "construct", Builtins::kReflectConstruct, 2, false);
    native_context()->set_reflect_construct(*construct);

    SimpleInstallFunction(isolate_, reflect, "get", Builtins::kReflectGet, 2,
                          false);
    SimpleInstallFunction(isolate_, reflect, "getOwnPropertyDescriptor",
                          Builtins::kReflectGetOwnPropertyDescriptor, 2, true);
    SimpleInstallFunction(isolate_, reflect, "getPrototypeOf",
                          Builtins::kReflectGetPrototypeOf, 1, true);
    SimpleInstallFunction(isolate_, reflect, "has", Builtins::kReflectHas, 2,
                          true);
    SimpleInstallFunction(isolate_, reflect, "isExtensible",
                          Builtins::kReflectIsExtensible, 1, true);
    SimpleInstallFunction(isolate_, reflect, "ownKeys",
                          Builtins::kReflectOwnKeys, 1, true);
    SimpleInstallFunction(isolate_, reflect, "preventExtensions",
                          Builtins::kReflectPreventExtensions, 1, true);
    SimpleInstallFunction(isolate_, reflect, "set", Builtins::kReflectSet, 3,
                          false);
    SimpleInstallFunction(isolate_, reflect, "setPrototypeOf",
                          Builtins::kReflectSetPrototypeOf, 2, true);
  }

  {  // --- B o u n d F u n c t i o n
    Handle<Map> map =
        factory->NewMap(JS_BOUND_FUNCTION_TYPE, JSBoundFunction::kHeaderSize,
                        TERMINAL_FAST_ELEMENTS_KIND, 0);
    map->SetConstructor(native_context()->object_function());
    map->set_is_callable(true);
    Map::SetPrototype(isolate(), map, empty_function);

    PropertyAttributes roc_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->bound_function_length_accessor(),
          roc_attribs);
      map->AppendDescriptor(isolate(), &d);
    }

    {  // name
      Descriptor d = Descriptor::AccessorConstant(
          factory->name_string(), factory->bound_function_name_accessor(),
          roc_attribs);
      map->AppendDescriptor(isolate(), &d);
    }
    native_context()->set_bound_function_without_constructor_map(*map);

    map = Map::Copy(isolate_, map, "IsConstructor");
    map->set_is_constructor(true);
    native_context()->set_bound_function_with_constructor_map(*map);
  }

  {  // --- sloppy arguments map
    Handle<String> arguments_string = factory->Arguments_string();
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        arguments_string, isolate_->initial_object_prototype(),
        JS_ARGUMENTS_OBJECT_TYPE, JSSloppyArgumentsObject::kSize, 2,
        Builtins::kIllegal, MUTABLE);
    Handle<JSFunction> function = factory->NewFunction(args);
    Handle<Map> map(function->initial_map(), isolate());

    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSSloppyArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // callee
      Descriptor d =
          Descriptor::DataField(isolate(), factory->callee_string(),
                                JSSloppyArgumentsObject::kCalleeIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    // @@iterator method is added later.

    native_context()->set_sloppy_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- fast and slow aliased arguments map
    Handle<Map> map = isolate_->sloppy_arguments_map();
    map = Map::Copy(isolate_, map, "FastAliasedArguments");
    map->set_elements_kind(FAST_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_fast_aliased_arguments_map(*map);

    map = Map::Copy(isolate_, map, "SlowAliasedArguments");
    map->set_elements_kind(SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_slow_aliased_arguments_map(*map);
  }

  {  // --- strict mode arguments map
    const PropertyAttributes attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError function.
    Handle<AccessorPair> callee = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetThrowTypeErrorIntrinsic();

    // Install the ThrowTypeError function.
    callee->set_getter(*poison);
    callee->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map =
        factory->NewMap(JS_ARGUMENTS_OBJECT_TYPE,
                        JSStrictArgumentsObject::kSize, PACKED_ELEMENTS, 1);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSStrictArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // callee
      Descriptor d = Descriptor::AccessorConstant(factory->callee_string(),
                                                  callee, attributes);
      map->AppendDescriptor(isolate(), &d);
    }
    // @@iterator method is added later.

    DCHECK_EQ(native_context()->object_function().prototype(),
              *isolate_->initial_object_prototype());
    Map::SetPrototype(isolate(), map, isolate_->initial_object_prototype());

    // Copy constructor from the sloppy arguments boilerplate.
    map->SetConstructor(
        native_context()->sloppy_arguments_map().GetConstructor());

    native_context()->set_strict_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<JSFunction> context_extension_fun =
        CreateFunction(isolate_, factory->empty_string(),
                       JS_CONTEXT_EXTENSION_OBJECT_TYPE, JSObject::kHeaderSize,
                       0, factory->the_hole_value(), Builtins::kIllegal);
    native_context()->set_context_extension_function(*context_extension_fun);
  }

  {
    // Set up the call-as-function delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kHandleApiCallAsFunction, 0, false);
    native_context()->set_call_as_function_delegate(*delegate);
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kHandleApiCallAsConstructor, 0, false);
    native_context()->set_call_as_constructor_delegate(*delegate);
  }
}  // NOLINT(readability/fn_size)

Handle<JSFunction> Genesis::InstallTypedArray(const char* name,
                                              ElementsKind elements_kind) {
  Handle<JSObject> global =
      Handle<JSObject>(native_context()->global_object(), isolate());

  Handle<JSObject> typed_array_prototype = isolate()->typed_array_prototype();
  Handle<JSFunction> typed_array_function = isolate()->typed_array_function();

  Handle<JSFunction> result = InstallFunction(
      isolate(), global, name, JS_TYPED_ARRAY_TYPE,
      JSTypedArray::kSizeWithEmbedderFields, 0, factory()->the_hole_value(),
      Builtins::kTypedArrayConstructor);
  result->initial_map().set_elements_kind(elements_kind);

  result->shared().DontAdaptArguments();
  result->shared().set_length(3);

  CHECK(JSObject::SetPrototype(result, typed_array_function, false, kDontThrow)
            .FromJust());

  Handle<Smi> bytes_per_element(
      Smi::FromInt(1 << ElementsKindToShiftSize(elements_kind)), isolate());

  InstallConstant(isolate(), result, "BYTES_PER_ELEMENT", bytes_per_element);

  // Setup prototype object.
  DCHECK(result->prototype().IsJSObject());
  Handle<JSObject> prototype(JSObject::cast(result->prototype()), isolate());

  CHECK(JSObject::SetPrototype(prototype, typed_array_prototype, false,
                               kDontThrow)
            .FromJust());

  InstallConstant(isolate(), prototype, "BYTES_PER_ELEMENT", bytes_per_element);
  return result;
}

void Genesis::InitializeExperimentalGlobal() {
#define FEATURE_INITIALIZE_GLOBAL(id, descr) InitializeGlobal_##id();

  HARMONY_INPROGRESS(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_STAGED(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_SHIPPING(FEATURE_INITIALIZE_GLOBAL)
#undef FEATURE_INITIALIZE_GLOBAL
}

bool Genesis::CompileExtension(Isolate* isolate, v8::Extension* extension) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> function_info;

  Handle<String> source =
      isolate->factory()
          ->NewExternalStringFromOneByte(extension->source())
          .ToHandleChecked();
  DCHECK(source->IsOneByteRepresentation());

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  Vector<const char> name = CStrVector(extension->name());
  SourceCodeCache* cache = isolate->bootstrapper()->extensions_cache();
  Handle<Context> context(isolate->context(), isolate);
  DCHECK(context->IsNativeContext());

  if (!cache->Lookup(isolate, name, &function_info)) {
    Handle<String> script_name =
        factory->NewStringFromUtf8(name).ToHandleChecked();
    MaybeHandle<SharedFunctionInfo> maybe_function_info =
        Compiler::GetSharedFunctionInfoForScript(
            isolate, source, Compiler::ScriptDetails(script_name),
            ScriptOriginOptions(), extension, nullptr,
            ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseV8Extension, EXTENSION_CODE);
    if (!maybe_function_info.ToHandle(&function_info)) return false;
    cache->Add(isolate, name, function_info);
  }

  // Set up the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  Handle<JSFunction> fun =
      factory->NewFunctionFromSharedFunctionInfo(function_info, context);

  // Call function using either the runtime object or the global
  // object as the receiver. Provide no parameters.
  Handle<Object> receiver = isolate->global_object();
  return !Execution::TryCall(isolate, fun, receiver, 0, nullptr,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}

void Genesis::InitializeIteratorFunctions() {
  Isolate* isolate = isolate_;
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = isolate->native_context();
  Handle<JSObject> iterator_prototype(
      native_context->initial_iterator_prototype(), isolate);

  {  // -- G e n e r a t o r
    PrototypeIterator iter(isolate, native_context->generator_function_map());
    Handle<JSObject> generator_function_prototype(iter.GetCurrent<JSObject>(),
                                                  isolate);
    Handle<JSFunction> generator_function_function = CreateFunction(
        isolate, "GeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, generator_function_prototype,
        Builtins::kGeneratorFunctionConstructor);
    generator_function_function->set_prototype_or_initial_map(
        native_context->generator_function_map());
    generator_function_function->shared().DontAdaptArguments();
    generator_function_function->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, generator_function_function,
        Context::GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(generator_function_function,
                                isolate->function_function());
    JSObject::AddProperty(
        isolate, generator_function_prototype, factory->constructor_string(),
        generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->generator_function_map().SetConstructor(
        *generator_function_function);
  }

  {  // -- A s y n c G e n e r a t o r
    PrototypeIterator iter(isolate,
                           native_context->async_generator_function_map());
    Handle<JSObject> async_generator_function_prototype(
        iter.GetCurrent<JSObject>(), isolate);

    Handle<JSFunction> async_generator_function_function = CreateFunction(
        isolate, "AsyncGeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_generator_function_prototype,
        Builtins::kAsyncGeneratorFunctionConstructor);
    async_generator_function_function->set_prototype_or_initial_map(
        native_context->async_generator_function_map());
    async_generator_function_function->shared().DontAdaptArguments();
    async_generator_function_function->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, async_generator_function_function,
        Context::ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(async_generator_function_function,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_generator_function_prototype,
        factory->constructor_string(), async_generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->async_generator_function_map().SetConstructor(
        *async_generator_function_function);
  }

  {  // -- S e t I t e r a t o r
    // Setup %SetIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(prototype, iterator_prototype);

    InstallToStringTag(isolate, prototype, factory->SetIterator_string());

    // Install the next function on the {prototype}.
    InstallFunctionWithBuiltinId(isolate, prototype, "next",
                                 Builtins::kSetIteratorPrototypeNext, 0, true);
    native_context->set_initial_set_iterator_prototype(*prototype);

    // Setup SetIterator constructor.
    Handle<JSFunction> set_iterator_function = CreateFunction(
        isolate, "SetIterator", JS_SET_VALUE_ITERATOR_TYPE,
        JSSetIterator::kHeaderSize, 0, prototype, Builtins::kIllegal);
    set_iterator_function->shared().set_native(false);

    Handle<Map> set_value_iterator_map(set_iterator_function->initial_map(),
                                       isolate);
    native_context->set_set_value_iterator_map(*set_value_iterator_map);

    Handle<Map> set_key_value_iterator_map = Map::Copy(
        isolate, set_value_iterator_map, "JS_SET_KEY_VALUE_ITERATOR_TYPE");
    set_key_value_iterator_map->set_instance_type(
        JS_SET_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_set_key_value_iterator_map(*set_key_value_iterator_map);
  }

  {  // -- M a p I t e r a t o r
    // Setup %MapIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(prototype, iterator_prototype);

    InstallToStringTag(isolate, prototype, factory->MapIterator_string());

    // Install the next function on the {prototype}.
    InstallFunctionWithBuiltinId(isolate, prototype, "next",
                                 Builtins::kMapIteratorPrototypeNext, 0, true);
    native_context->set_initial_map_iterator_prototype(*prototype);

    // Setup MapIterator constructor.
    Handle<JSFunction> map_iterator_function = CreateFunction(
        isolate, "MapIterator", JS_MAP_KEY_ITERATOR_TYPE,
        JSMapIterator::kHeaderSize, 0, prototype, Builtins::kIllegal);
    map_iterator_function->shared().set_native(false);

    Handle<Map> map_key_iterator_map(map_iterator_function->initial_map(),
                                     isolate);
    native_context->set_map_key_iterator_map(*map_key_iterator_map);

    Handle<Map> map_key_value_iterator_map = Map::Copy(
        isolate, map_key_iterator_map, "JS_MAP_KEY_VALUE_ITERATOR_TYPE");
    map_key_value_iterator_map->set_instance_type(
        JS_MAP_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_map_key_value_iterator_map(*map_key_value_iterator_map);

    Handle<Map> map_value_iterator_map =
        Map::Copy(isolate, map_key_iterator_map, "JS_MAP_VALUE_ITERATOR_TYPE");
    map_value_iterator_map->set_instance_type(JS_MAP_VALUE_ITERATOR_TYPE);
    native_context->set_map_value_iterator_map(*map_value_iterator_map);
  }

  {  // -- A s y n c F u n c t i o n
    // Builtin functions for AsyncFunction.
    PrototypeIterator iter(isolate, native_context->async_function_map());
    Handle<JSObject> async_function_prototype(iter.GetCurrent<JSObject>(),
                                              isolate);

    Handle<JSFunction> async_function_constructor = CreateFunction(
        isolate, "AsyncFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_function_prototype,
        Builtins::kAsyncFunctionConstructor);
    async_function_constructor->set_prototype_or_initial_map(
        native_context->async_function_map());
    async_function_constructor->shared().DontAdaptArguments();
    async_function_constructor->shared().set_length(1);
    native_context->set_async_function_constructor(*async_function_constructor);
    JSObject::ForceSetPrototype(async_function_constructor,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_function_prototype, factory->constructor_string(),
        async_function_constructor,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    JSFunction::SetPrototype(async_function_constructor,
                             async_function_prototype);

    // Async functions don't have a prototype, but they use generator objects
    // under the hood to model the suspend/resume (in await). Instead of using
    // the "prototype" / initial_map machinery (like for (async) generators),
    // there's one global (per native context) map here that is used for the
    // async function generator objects. These objects never escape to user
    // JavaScript anyways.
    Handle<Map> async_function_object_map = factory->NewMap(
        JS_ASYNC_FUNCTION_OBJECT_TYPE, JSAsyncFunctionObject::kHeaderSize);
    native_context->set_async_function_object_map(*async_function_object_map);

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate, Builtins::kAsyncFunctionAwaitRejectClosure,
          factory->empty_string(), 1);
      native_context->set_async_function_await_reject_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate, Builtins::kAsyncFunctionAwaitResolveClosure,
          factory->empty_string(), 1);
      native_context->set_async_function_await_resolve_shared_fun(*info);
    }
  }
}

void Genesis::InitializeCallSiteBuiltins() {
  Factory* factory = isolate()->factory();
  HandleScope scope(isolate());
  // -- C a l l S i t e
  // Builtin functions for CallSite.

  // CallSites are a special case; the constructor is for our private use
  // only, therefore we set it up as a builtin that throws. Internally, we use
  // CallSiteUtils::Construct to create CallSite objects.

  Handle<JSFunction> callsite_fun = CreateFunction(
      isolate(), "CallSite", JS_OBJECT_TYPE, JSObject::kHeaderSize, 0,
      factory->the_hole_value(), Builtins::kUnsupportedThrower);
  callsite_fun->shared().DontAdaptArguments();
  isolate()->native_context()->set_callsite_function(*callsite_fun);

  // Setup CallSite.prototype.
  Handle<JSObject> prototype(JSObject::cast(callsite_fun->instance_prototype()),
                             isolate());

  struct FunctionInfo {
    const char* name;
    Builtins::Name id;
  };

  FunctionInfo infos[] = {
      {"getColumnNumber", Builtins::kCallSitePrototypeGetColumnNumber},
      {"getEvalOrigin", Builtins::kCallSitePrototypeGetEvalOrigin},
      {"getFileName", Builtins::kCallSitePrototypeGetFileName},
      {"getFunction", Builtins::kCallSitePrototypeGetFunction},
      {"getFunctionName", Builtins::kCallSitePrototypeGetFunctionName},
      {"getLineNumber", Builtins::kCallSitePrototypeGetLineNumber},
      {"getMethodName", Builtins::kCallSitePrototypeGetMethodName},
      {"getPosition", Builtins::kCallSitePrototypeGetPosition},
      {"getPromiseIndex", Builtins::kCallSitePrototypeGetPromiseIndex},
      {"getScriptNameOrSourceURL",
       Builtins::kCallSitePrototypeGetScriptNameOrSourceURL},
      {"getThis", Builtins::kCallSitePrototypeGetThis},
      {"getTypeName", Builtins::kCallSitePrototypeGetTypeName},
      {"isAsync", Builtins::kCallSitePrototypeIsAsync},
      {"isConstructor", Builtins::kCallSitePrototypeIsConstructor},
      {"isEval", Builtins::kCallSitePrototypeIsEval},
      {"isNative", Builtins::kCallSitePrototypeIsNative},
      {"isPromiseAll", Builtins::kCallSitePrototypeIsPromiseAll},
      {"isToplevel", Builtins::kCallSitePrototypeIsToplevel},
      {"toString", Builtins::kCallSitePrototypeToString}};

  PropertyAttributes attrs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

  Handle<JSFunction> fun;
  for (const FunctionInfo& info : infos) {
    SimpleInstallFunction(isolate(), prototype, info.name, info.id, 0, true,
                          attrs);
  }
}

#define EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(id) \
  void Genesis::InitializeGlobal_##id() {}

EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_namespace_exports)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_private_methods)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_dynamic_import)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_import_meta)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_sequence)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_optional_chaining)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_nullish)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_top_level_await)

#ifdef V8_INTL_SUPPORT
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_add_calendar_numbering_system)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_displaynames_date_types)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_dateformat_day_period)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(
    harmony_intl_dateformat_fractional_second_digits)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_other_calendars)
#endif  // V8_INTL_SUPPORT

#undef EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE

void Genesis::InitializeGlobal_harmony_sharedarraybuffer() {
  if (!FLAG_harmony_sharedarraybuffer) return;

  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());

  JSObject::AddProperty(isolate_, global, "SharedArrayBuffer",
                        isolate()->shared_array_buffer_fun(), DONT_ENUM);

  JSObject::AddProperty(isolate_, global, "Atomics",
                        isolate()->atomics_object(), DONT_ENUM);
  InstallToStringTag(isolate_, isolate()->atomics_object(), "Atomics");
}

void Genesis::InitializeGlobal_harmony_weak_refs() {
  if (!FLAG_harmony_weak_refs) return;

  Factory* factory = isolate()->factory();
  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());

  {
    // Create %FinalizationRegistryPrototype%
    Handle<String> finalization_registry_name =
        factory->NewStringFromStaticChars("FinalizationRegistry");
    Handle<JSObject> finalization_registry_prototype = factory->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);

    // Create %FinalizationRegistry%
    Handle<JSFunction> finalization_registry_fun = CreateFunction(
        isolate(), finalization_registry_name, JS_FINALIZATION_REGISTRY_TYPE,
        JSFinalizationRegistry::kHeaderSize, 0, finalization_registry_prototype,
        Builtins::kFinalizationRegistryConstructor);
    InstallWithIntrinsicDefaultProto(
        isolate(), finalization_registry_fun,
        Context::JS_FINALIZATION_REGISTRY_FUNCTION_INDEX);

    finalization_registry_fun->shared().DontAdaptArguments();
    finalization_registry_fun->shared().set_length(1);

    // Install the "constructor" property on the prototype.
    JSObject::AddProperty(isolate(), finalization_registry_prototype,
                          factory->constructor_string(),
                          finalization_registry_fun, DONT_ENUM);

    InstallToStringTag(isolate(), finalization_registry_prototype,
                       finalization_registry_name);

    JSObject::AddProperty(isolate(), global, finalization_registry_name,
                          finalization_registry_fun, DONT_ENUM);

    SimpleInstallFunction(isolate(), finalization_registry_prototype,
                          "register", Builtins::kFinalizationRegistryRegister,
                          2, false);

    SimpleInstallFunction(isolate(), finalization_registry_prototype,
                          "unregister",
                          Builtins::kFinalizationRegistryUnregister, 1, false);

    SimpleInstallFunction(isolate(), finalization_registry_prototype,
                          "cleanupSome",
                          Builtins::kFinalizationRegistryCleanupSome, 0, false);
  }
  {
    // Create %WeakRefPrototype%
    Handle<Map> weak_ref_map =
        factory->NewMap(JS_WEAK_REF_TYPE, JSWeakRef::kHeaderSize);
    DCHECK(weak_ref_map->IsJSObjectMap());

    Handle<JSObject> weak_ref_prototype = factory->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);
    Map::SetPrototype(isolate(), weak_ref_map, weak_ref_prototype);

    InstallToStringTag(isolate(), weak_ref_prototype,
                       factory->WeakRef_string());

    SimpleInstallFunction(isolate(), weak_ref_prototype, "deref",
                          Builtins::kWeakRefDeref, 0, false);

    // Create %WeakRef%
    Handle<String> weak_ref_name = factory->InternalizeUtf8String("WeakRef");
    Handle<JSFunction> weak_ref_fun = CreateFunction(
        isolate(), weak_ref_name, JS_WEAK_REF_TYPE, JSWeakRef::kHeaderSize, 0,
        weak_ref_prototype, Builtins::kWeakRefConstructor);
    InstallWithIntrinsicDefaultProto(isolate(), weak_ref_fun,
                                     Context::JS_WEAK_REF_FUNCTION_INDEX);

    weak_ref_fun->shared().DontAdaptArguments();
    weak_ref_fun->shared().set_length(1);

    // Install the "constructor" property on the prototype.
    JSObject::AddProperty(isolate(), weak_ref_prototype,
                          factory->constructor_string(), weak_ref_fun,
                          DONT_ENUM);

    JSObject::AddProperty(isolate(), global, weak_ref_name, weak_ref_fun,
                          DONT_ENUM);
  }

  {
    // Create cleanup iterator for JSFinalizationRegistry.
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> cleanup_iterator_prototype = factory->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(cleanup_iterator_prototype, iterator_prototype);

    InstallToStringTag(isolate(), cleanup_iterator_prototype,
                       "FinalizationRegistry Cleanup Iterator");

    SimpleInstallFunction(isolate(), cleanup_iterator_prototype, "next",
                          Builtins::kFinalizationRegistryCleanupIteratorNext, 0,
                          true);
    Handle<Map> cleanup_iterator_map =
        factory->NewMap(JS_FINALIZATION_REGISTRY_CLEANUP_ITERATOR_TYPE,
                        JSFinalizationRegistryCleanupIterator::kHeaderSize);
    Map::SetPrototype(isolate(), cleanup_iterator_map,
                      cleanup_iterator_prototype);
    native_context()->set_js_finalization_registry_cleanup_iterator_map(
        *cleanup_iterator_map);
  }
}

void Genesis::InitializeGlobal_harmony_promise_all_settled() {
  if (!FLAG_harmony_promise_all_settled) return;
  SimpleInstallFunction(isolate(), isolate()->promise_function(), "allSettled",
                        Builtins::kPromiseAllSettled, 1, true);
  Factory* factory = isolate()->factory();
  {
    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAllSettledResolveElementClosure,
        factory->empty_string(), 1);
    native_context()->set_promise_all_settled_resolve_element_shared_fun(*info);
  }

  {
    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kPromiseAllSettledRejectElementClosure,
        factory->empty_string(), 1);
    native_context()->set_promise_all_settled_reject_element_shared_fun(*info);
  }
}

void Genesis::InitializeGlobal_harmony_regexp_match_indices() {
  if (!FLAG_harmony_regexp_match_indices) return;

  // Add indices accessor to JSRegExpResult's initial map.
  Handle<Map> initial_map(native_context()->regexp_result_map(), isolate());
  Descriptor d = Descriptor::AccessorConstant(
      factory()->indices_string(), factory()->regexp_result_indices_accessor(),
      NONE);
  Map::EnsureDescriptorSlack(isolate(), initial_map, 1);
  initial_map->AppendDescriptor(isolate(), &d);
}

void Genesis::InitializeGlobal_harmony_string_replaceall() {
  if (!FLAG_harmony_string_replaceall) return;

  Handle<JSFunction> string_fun(native_context()->string_function(), isolate());
  Handle<JSObject> string_prototype(
      JSObject::cast(string_fun->instance_prototype()), isolate());

  SimpleInstallFunction(isolate(), string_prototype, "replaceAll",
                        Builtins::kStringPrototypeReplaceAll, 2, true);
}

#ifdef V8_INTL_SUPPORT

void Genesis::InitializeGlobal_harmony_intl_segmenter() {
  if (!FLAG_harmony_intl_segmenter) return;
  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> segmenter_fun = InstallFunction(
      isolate(), intl, "Segmenter", JS_SEGMENTER_TYPE, JSSegmenter::kHeaderSize,
      0, factory()->the_hole_value(), Builtins::kSegmenterConstructor);
  segmenter_fun->shared().set_length(0);
  segmenter_fun->shared().DontAdaptArguments();
  InstallWithIntrinsicDefaultProto(isolate_, segmenter_fun,
                                   Context::INTL_SEGMENTER_FUNCTION_INDEX);

  SimpleInstallFunction(isolate(), segmenter_fun, "supportedLocalesOf",
                        Builtins::kSegmenterSupportedLocalesOf, 1, false);

  {
    // Setup %SegmenterPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(segmenter_fun->instance_prototype()), isolate());

    InstallToStringTag(isolate(), prototype, "Intl.Segmenter");

    SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                          Builtins::kSegmenterPrototypeResolvedOptions, 0,
                          false);

    SimpleInstallFunction(isolate(), prototype, "segment",
                          Builtins::kSegmenterPrototypeSegment, 1, false);
  }

  {
    // Setup %SegmentIteratorPrototype%.
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> prototype = factory()->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(prototype, iterator_prototype);

    InstallToStringTag(isolate(), prototype,
                       factory()->SegmentIterator_string());

    SimpleInstallFunction(isolate(), prototype, "next",
                          Builtins::kSegmentIteratorPrototypeNext, 0, false);

    SimpleInstallFunction(isolate(), prototype, "following",
                          Builtins::kSegmentIteratorPrototypeFollowing, 0,
                          false);

    SimpleInstallFunction(isolate(), prototype, "preceding",
                          Builtins::kSegmentIteratorPrototypePreceding, 0,
                          false);

    SimpleInstallGetter(isolate(), prototype, factory()->index_string(),
                        Builtins::kSegmentIteratorPrototypeIndex, false);

    SimpleInstallGetter(isolate(), prototype, factory()->breakType_string(),
                        Builtins::kSegmentIteratorPrototypeBreakType, false);

    // Setup SegmentIterator constructor.
    Handle<String> name_string =
        Name::ToFunctionName(isolate(),
                             isolate()->factory()->SegmentIterator_string())
            .ToHandleChecked();
    Handle<JSFunction> segment_iterator_fun = CreateFunction(
        isolate(), name_string, JS_SEGMENT_ITERATOR_TYPE,
        JSSegmentIterator::kHeaderSize, 0, prototype, Builtins::kIllegal);
    segment_iterator_fun->shared().set_native(false);

    Handle<Map> segment_iterator_map(segment_iterator_fun->initial_map(),
                                     isolate());
    native_context()->set_intl_segment_iterator_map(*segment_iterator_map);
  }
}

void Genesis::InitializeGlobal_harmony_intl_displaynames() {
  if (!FLAG_harmony_intl_displaynames) return;
  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> display_names_fun = InstallFunction(
      isolate(), intl, "DisplayNames", JS_DISPLAY_NAMES_TYPE,
      JSDisplayNames::kHeaderSize, 0, factory()->the_hole_value(),
      Builtins::kDisplayNamesConstructor);
  display_names_fun->shared().set_length(0);
  display_names_fun->shared().DontAdaptArguments();
  InstallWithIntrinsicDefaultProto(isolate_, display_names_fun,
                                   Context::INTL_DISPLAY_NAMES_FUNCTION_INDEX);

  SimpleInstallFunction(isolate(), display_names_fun, "supportedLocalesOf",
                        Builtins::kDisplayNamesSupportedLocalesOf, 1, false);

  {
    // Setup %DisplayNamesPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(display_names_fun->instance_prototype()), isolate());

    InstallToStringTag(isolate(), prototype, "Intl.DisplayNames");

    SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                          Builtins::kDisplayNamesPrototypeResolvedOptions, 0,
                          false);

    SimpleInstallFunction(isolate(), prototype, "of",
                          Builtins::kDisplayNamesPrototypeOf, 1, false);
  }
}

#endif  // V8_INTL_SUPPORT

Handle<JSFunction> Genesis::CreateArrayBuffer(
    Handle<String> name, ArrayBufferKind array_buffer_kind) {
  // Create the %ArrayBufferPrototype%
  // Setup the {prototype} with the given {name} for @@toStringTag.
  Handle<JSObject> prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  InstallToStringTag(isolate(), prototype, name);

  // Allocate the constructor with the given {prototype}.
  Handle<JSFunction> array_buffer_fun =
      CreateFunction(isolate(), name, JS_ARRAY_BUFFER_TYPE,
                     JSArrayBuffer::kSizeWithEmbedderFields, 0, prototype,
                     Builtins::kArrayBufferConstructor);
  array_buffer_fun->shared().DontAdaptArguments();
  array_buffer_fun->shared().set_length(1);

  // Install the "constructor" property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->constructor_string(),
                        array_buffer_fun, DONT_ENUM);

  switch (array_buffer_kind) {
    case ARRAY_BUFFER:
      InstallFunctionWithBuiltinId(isolate(), array_buffer_fun, "isView",
                                   Builtins::kArrayBufferIsView, 1, true);

      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtins::kArrayBufferPrototypeGetByteLength, false);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtins::kArrayBufferPrototypeSlice, 2, true);
      break;

    case SHARED_ARRAY_BUFFER:
      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtins::kSharedArrayBufferPrototypeGetByteLength,
                          false);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtins::kSharedArrayBufferPrototypeSlice, 2,
                            true);
      break;
  }

  return array_buffer_fun;
}

// TODO(jgruber): Refactor this into some kind of meaningful organization. There
// is likely no reason remaining for these objects to be installed here. For
// example, global object setup done in this function could likely move to
// InitializeGlobal.
bool Genesis::InstallABunchOfRandomThings() {
  HandleScope scope(isolate());

  auto fast_template_instantiations_cache = isolate()->factory()->NewFixedArray(
      TemplateInfo::kFastTemplateInstantiationsCacheSize);
  native_context()->set_fast_template_instantiations_cache(
      *fast_template_instantiations_cache);

  auto slow_template_instantiations_cache = SimpleNumberDictionary::New(
      isolate(), ApiNatives::kInitialFunctionCacheSize);
  native_context()->set_slow_template_instantiations_cache(
      *slow_template_instantiations_cache);

  // Store the map for the %ObjectPrototype% after the natives has been compiled
  // and the Object function has been set up.
  {
    Handle<JSFunction> object_function(native_context()->object_function(),
                                       isolate());
    DCHECK(JSObject::cast(object_function->initial_map().prototype())
               .HasFastProperties());
    native_context()->set_object_function_prototype_map(
        HeapObject::cast(object_function->initial_map().prototype()).map());
  }

  // Store the map for the %StringPrototype% after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function(),
                                     isolate());
  JSObject string_function_prototype =
      JSObject::cast(string_function->initial_map().prototype());
  DCHECK(string_function_prototype.HasFastProperties());
  native_context()->set_string_function_prototype_map(
      string_function_prototype.map());

  Handle<JSGlobalObject> global_object =
      handle(native_context()->global_object(), isolate());

  // Install Global.decodeURI.
  InstallFunctionWithBuiltinId(isolate(), global_object, "decodeURI",
                               Builtins::kGlobalDecodeURI, 1, false);

  // Install Global.decodeURIComponent.
  InstallFunctionWithBuiltinId(isolate(), global_object, "decodeURIComponent",
                               Builtins::kGlobalDecodeURIComponent, 1, false);

  // Install Global.encodeURI.
  InstallFunctionWithBuiltinId(isolate(), global_object, "encodeURI",
                               Builtins::kGlobalEncodeURI, 1, false);

  // Install Global.encodeURIComponent.
  InstallFunctionWithBuiltinId(isolate(), global_object, "encodeURIComponent",
                               Builtins::kGlobalEncodeURIComponent, 1, false);

  // Install Global.escape.
  InstallFunctionWithBuiltinId(isolate(), global_object, "escape",
                               Builtins::kGlobalEscape, 1, false);

  // Install Global.unescape.
  InstallFunctionWithBuiltinId(isolate(), global_object, "unescape",
                               Builtins::kGlobalUnescape, 1, false);

  // Install Global.eval.
  {
    Handle<JSFunction> eval = SimpleInstallFunction(
        isolate(), global_object, "eval", Builtins::kGlobalEval, 1, false);
    native_context()->set_global_eval_fun(*eval);
  }

  // Install Global.isFinite
  InstallFunctionWithBuiltinId(isolate(), global_object, "isFinite",
                               Builtins::kGlobalIsFinite, 1, true);

  // Install Global.isNaN
  InstallFunctionWithBuiltinId(isolate(), global_object, "isNaN",
                               Builtins::kGlobalIsNaN, 1, true);

  // Install Array builtin functions.
  {
    Handle<JSFunction> array_constructor(native_context()->array_function(),
                                         isolate());
    Handle<JSArray> proto(JSArray::cast(array_constructor->prototype()),
                          isolate());

    // Verification of important array prototype properties.
    Object length = proto->length();
    CHECK(length.IsSmi());
    CHECK_EQ(Smi::ToInt(length), 0);
    CHECK(proto->HasSmiOrObjectElements());
    // This is necessary to enable fast checks for absence of elements
    // on Array.prototype and below.
    proto->set_elements(ReadOnlyRoots(heap()).empty_fixed_array());
  }

  // Create a map for accessor property descriptors (a variant of JSObject
  // that predefines four properties get, set, configurable and enumerable).
  {
    // AccessorPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSAccessorPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // get
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->get_string(),
                                JSAccessorPropertyDescriptor::kGetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // set
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->set_string(),
                                JSAccessorPropertyDescriptor::kSetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSAccessorPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // configurable
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->configurable_string(),
          JSAccessorPropertyDescriptor::kConfigurableIndex, NONE,
          Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_accessor_property_descriptor_map(*map);
  }

  // Create a map for data property descriptors (a variant of JSObject
  // that predefines four properties value, writable, configurable and
  // enumerable).
  {
    // DataPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSDataPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // value
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->value_string(),
                                JSDataPropertyDescriptor::kValueIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // writable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->writable_string(),
                                JSDataPropertyDescriptor::kWritableIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSDataPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // configurable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->configurable_string(),
                                JSDataPropertyDescriptor::kConfigurableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_data_property_descriptor_map(*map);
  }

  // Create a constructor for RegExp results (a variant of Array that
  // predefines the properties index, input, and groups).
  {
    // JSRegExpResult initial map.
    // Add additional slack to the initial map in case regexp_match_indices
    // are enabled to account for the additional descriptor.
    Handle<Map> initial_map = CreateInitialMapForArraySubclass(
        JSRegExpResult::kSize, JSRegExpResult::kInObjectPropertyCount);

    // index descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->index_string(),
                                           JSRegExpResult::kIndexIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // input descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->input_string(),
                                           JSRegExpResult::kInputIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // groups descriptor.
    {
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->groups_string(), JSRegExpResult::kGroupsIndex,
          NONE, Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // Private internal only fields. All of the remaining fields have special
    // symbols to prevent their use in Javascript.
    {
      PropertyAttributes attribs = DONT_ENUM;

      // cached_indices_or_regexp descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(),
            factory()->regexp_result_cached_indices_or_regexp_symbol(),
            JSRegExpResult::kCachedIndicesOrRegExpIndex, attribs,
            Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }

      // names descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_names_symbol(),
            JSRegExpResult::kNamesIndex, attribs, Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }

      // regexp_input_index descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_regexp_input_symbol(),
            JSRegExpResult::kRegExpInputIndex, attribs,
            Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }

      // regexp_last_index descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_regexp_last_index_symbol(),
            JSRegExpResult::kRegExpLastIndex, attribs,
            Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }
    }

    native_context()->set_regexp_result_map(*initial_map);
  }

  // Create a constructor for JSRegExpResultIndices (a variant of Array that
  // predefines the groups property).
  {
    // JSRegExpResultIndices initial map.
    Handle<Map> initial_map = CreateInitialMapForArraySubclass(
        JSRegExpResultIndices::kSize,
        JSRegExpResultIndices::kInObjectPropertyCount);

    // groups descriptor.
    {
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->groups_string(),
          JSRegExpResultIndices::kGroupsIndex, NONE, Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
      DCHECK_EQ(initial_map->LastAdded().as_int(),
                JSRegExpResultIndices::kGroupsDescriptorIndex);
    }

    native_context()->set_regexp_result_indices_map(*initial_map);
  }

  // Add @@iterator method to the arguments object maps.
  {
    PropertyAttributes attribs = DONT_ENUM;
    Handle<AccessorInfo> arguments_iterator =
        factory()->arguments_iterator_accessor();
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->sloppy_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->fast_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->slow_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->strict_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
  }

  return true;
}

bool Genesis::InstallExtrasBindings() {
  HandleScope scope(isolate());

  Handle<JSObject> extras_binding = factory()->NewJSObjectWithNullProto();

  // binding.isTraceCategoryEnabled(category)
  SimpleInstallFunction(isolate(), extras_binding, "isTraceCategoryEnabled",
                        Builtins::kIsTraceCategoryEnabled, 1, true);

  // binding.trace(phase, category, name, id, data)
  SimpleInstallFunction(isolate(), extras_binding, "trace", Builtins::kTrace, 5,
                        true);

  native_context()->set_extras_binding_object(*extras_binding);

  return true;
}

void Genesis::InitializeNormalizedMapCaches() {
  Handle<NormalizedMapCache> cache = NormalizedMapCache::New(isolate());
  native_context()->set_normalized_map_cache(*cache);
}

bool Bootstrapper::InstallExtensions(Handle<Context> native_context,
                                     v8::ExtensionConfiguration* extensions) {
  // Don't install extensions into the snapshot.
  if (isolate_->serializer_enabled()) return true;
  BootstrapperActive active(this);
  SaveAndSwitchContext saved_context(isolate_, *native_context);
  return Genesis::InstallExtensions(isolate_, native_context, extensions) &&
         Genesis::InstallSpecialObjects(isolate_, native_context);
}

bool Genesis::InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context) {
  HandleScope scope(isolate);

  Handle<JSObject> Error = isolate->error_function();
  Handle<String> name = isolate->factory()->stackTraceLimit_string();
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(isolate, Error, name, stack_trace_limit, NONE);

  if (FLAG_expose_wasm) {
    // Install the internal data structures into the isolate and expose on
    // the global object.
    WasmJs::Install(isolate, true);
  } else if (FLAG_validate_asm) {
    // Install the internal data structures only; these are needed for asm.js
    // translated to Wasm to work correctly.
    WasmJs::Install(isolate, false);
  }

  return true;
}

static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}

Genesis::ExtensionStates::ExtensionStates() : map_(8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  base::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension));
  if (entry == nullptr) {
    return UNVISITED;
  }
  return static_cast<ExtensionTraversalState>(
      reinterpret_cast<intptr_t>(entry->value));
}

void Genesis::ExtensionStates::set_state(RegisteredExtension* extension,
                                         ExtensionTraversalState state) {
  map_.LookupOrInsert(extension, Hash(extension))->value =
      reinterpret_cast<void*>(static_cast<intptr_t>(state));
}

bool Genesis::InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions) {
  ExtensionStates extension_states;  // All extensions have state UNVISITED.
  return InstallAutoExtensions(isolate, &extension_states) &&
         (!FLAG_expose_gc ||
          InstallExtension(isolate, "v8/gc", &extension_states)) &&
         (!FLAG_expose_externalize_string ||
          InstallExtension(isolate, "v8/externalize", &extension_states)) &&
         (!TracingFlags::is_gc_stats_enabled() ||
          InstallExtension(isolate, "v8/statistics", &extension_states)) &&
         (!FLAG_expose_trigger_failure ||
          InstallExtension(isolate, "v8/trigger-failure", &extension_states)) &&
         (!FLAG_trace_ignition_dispatches ||
          InstallExtension(isolate, "v8/ignition-statistics",
                           &extension_states)) &&
         (!isValidCpuTraceMarkFunctionName() ||
          InstallExtension(isolate, "v8/cpumark", &extension_states)) &&
#ifdef ENABLE_VTUNE_TRACEMARK
         (!FLAG_enable_vtune_domain_support ||
          InstallExtension(isolate, "v8/vtunedomain", &extension_states)) &&
#endif  // ENABLE_VTUNE_TRACEMARK
         InstallRequestedExtensions(isolate, extensions, &extension_states);
}

bool Genesis::InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (it->extension()->auto_enable() &&
        !InstallExtension(isolate, it, extension_states)) {
      return false;
    }
  }
  return true;
}

bool Genesis::InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states) {
  for (const char** it = extensions->begin(); it != extensions->end(); ++it) {
    if (!InstallExtension(isolate, *it, extension_states)) return false;
  }
  return true;
}

// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(Isolate* isolate, const char* name,
                               ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (strcmp(name, it->extension()->name()) == 0) {
      return InstallExtension(isolate, it, extension_states);
    }
  }
  return Utils::ApiCheck(false, "v8::Context::New()",
                         "Cannot find required extension");
}

bool Genesis::InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states) {
  HandleScope scope(isolate);

  if (extension_states->get_state(current) == INSTALLED) return true;
  // The current node has already been visited so there must be a
  // cycle in the dependency graph; fail.
  if (!Utils::ApiCheck(extension_states->get_state(current) != VISITED,
                       "v8::Context::New()", "Circular extension dependency")) {
    return false;
  }
  DCHECK(extension_states->get_state(current) == UNVISITED);
  extension_states->set_state(current, VISITED);
  v8::Extension* extension = current->extension();
  // Install the extension's dependencies
  for (int i = 0; i < extension->dependency_count(); i++) {
    if (!InstallExtension(isolate, extension->dependencies()[i],
                          extension_states)) {
      return false;
    }
  }
  // We do not expect this to throw an exception. Change this if it does.
  bool result = CompileExtension(isolate, extension);
  DCHECK(isolate->has_pending_exception() != result);
  if (!result) {
    // We print out the name of the extension that fail to install.
    // When an error is thrown during bootstrapping we automatically print
    // the line number at which this happened to the console in the isolate
    // error throwing functionality.
    base::OS::PrintError("Error installing extension '%s'.\n",
                         current->extension()->name());
    isolate->clear_pending_exception();
  }
  extension_states->set_state(current, INSTALLED);
  return result;
}

bool Genesis::ConfigureGlobalObjects(
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  Handle<JSObject> global_proxy(native_context()->global_proxy(), isolate());
  Handle<JSObject> global_object(native_context()->global_object(), isolate());

  if (!global_proxy_template.IsEmpty()) {
    // Configure the global proxy object.
    Handle<ObjectTemplateInfo> global_proxy_data =
        v8::Utils::OpenHandle(*global_proxy_template);
    if (!ConfigureApiObject(global_proxy, global_proxy_data)) return false;

    // Configure the global object.
    Handle<FunctionTemplateInfo> proxy_constructor(
        FunctionTemplateInfo::cast(global_proxy_data->constructor()),
        isolate());
    if (!proxy_constructor->GetPrototypeTemplate().IsUndefined(isolate())) {
      Handle<ObjectTemplateInfo> global_object_data(
          ObjectTemplateInfo::cast(proxy_constructor->GetPrototypeTemplate()),
          isolate());
      if (!ConfigureApiObject(global_object, global_object_data)) return false;
    }
  }

  JSObject::ForceSetPrototype(global_proxy, global_object);

  native_context()->set_array_buffer_map(
      native_context()->array_buffer_fun().initial_map());

  Handle<JSFunction> js_map_fun(native_context()->js_map_fun(), isolate());
  Handle<JSFunction> js_set_fun(native_context()->js_set_fun(), isolate());
  // Force the Map/Set constructor to fast properties, so that we can use the
  // fast paths for various things like
  //
  //   x instanceof Map
  //   x instanceof Set
  //
  // etc. We should probably come up with a more principled approach once
  // the JavaScript builtins are gone.
  JSObject::MigrateSlowToFast(js_map_fun, 0, "Bootstrapping");
  JSObject::MigrateSlowToFast(js_set_fun, 0, "Bootstrapping");

  native_context()->set_js_map_map(js_map_fun->initial_map());
  native_context()->set_js_set_map(js_set_fun->initial_map());

  return true;
}

bool Genesis::ConfigureApiObject(Handle<JSObject> object,
                                 Handle<ObjectTemplateInfo> object_template) {
  DCHECK(!object_template.is_null());
  DCHECK(FunctionTemplateInfo::cast(object_template->constructor())
             .IsTemplateFor(object->map()));

  MaybeHandle<JSObject> maybe_obj =
      ApiNatives::InstantiateObject(object->GetIsolate(), object_template);
  Handle<JSObject> instantiated_template;
  if (!maybe_obj.ToHandle(&instantiated_template)) {
    DCHECK(isolate()->has_pending_exception());
    isolate()->clear_pending_exception();
    return false;
  }
  TransferObject(instantiated_template, object);
  return true;
}

static bool PropertyAlreadyExists(Isolate* isolate, Handle<JSObject> to,
                                  Handle<Name> key) {
  LookupIterator it(isolate, to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
  return it.IsFound();
}

void Genesis::TransferNamedProperties(Handle<JSObject> from,
                                      Handle<JSObject> to) {
  // If JSObject::AddProperty asserts due to already existing property,
  // it is likely due to both global objects sharing property name(s).
  // Merging those two global objects is impossible.
  // The global template must not create properties that already exist
  // in the snapshotted global object.
  if (from->HasFastProperties()) {
    Handle<DescriptorArray> descs =
        Handle<DescriptorArray>(from->map().instance_descriptors(), isolate());
    for (InternalIndex i : from->map().IterateOwnDescriptors()) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        if (details.kind() == kData) {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i), isolate());
          // If the property is already there we skip it.
          if (PropertyAlreadyExists(isolate(), to, key)) continue;
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          Handle<Object> value =
              JSObject::FastPropertyAt(from, details.representation(), index);
          JSObject::AddProperty(isolate(), to, key, value,
                                details.attributes());
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          UNREACHABLE();
        }

      } else {
        DCHECK_EQ(kDescriptor, details.location());
        DCHECK_EQ(kAccessor, details.kind());
        Handle<Name> key(descs->GetKey(i), isolate());
        // If the property is already there we skip it.
        if (PropertyAlreadyExists(isolate(), to, key)) continue;
        HandleScope inner(isolate());
        DCHECK(!to->HasFastProperties());
        // Add to dictionary.
        Handle<Object> value(descs->GetStrongValue(i), isolate());
        PropertyDetails d(kAccessor, details.attributes(),
                          PropertyCellType::kMutable);
        JSObject::SetNormalizedProperty(to, key, value, d);
      }
    }
  } else if (from->IsJSGlobalObject()) {
    // Copy all keys and values in enumeration order.
    Handle<GlobalDictionary> properties(
        JSGlobalObject::cast(*from).global_dictionary(), isolate());
    Handle<FixedArray> indices =
        GlobalDictionary::IterationIndices(isolate(), properties);
    for (int i = 0; i < indices->length(); i++) {
      InternalIndex index(Smi::ToInt(indices->get(i)));
      Handle<PropertyCell> cell(properties->CellAt(index), isolate());
      Handle<Name> key(cell->name(), isolate());
      // If the property is already there we skip it.
      if (PropertyAlreadyExists(isolate(), to, key)) continue;
      // Set the property.
      Handle<Object> value(cell->value(), isolate());
      if (value->IsTheHole(isolate())) continue;
      PropertyDetails details = cell->property_details();
      if (details.kind() != kData) continue;
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  } else {
    // Copy all keys and values in enumeration order.
    Handle<NameDictionary> properties =
        Handle<NameDictionary>(from->property_dictionary(), isolate());
    Handle<FixedArray> key_indices =
        NameDictionary::IterationIndices(isolate(), properties);
    ReadOnlyRoots roots(isolate());
    for (int i = 0; i < key_indices->length(); i++) {
      InternalIndex key_index(Smi::ToInt(key_indices->get(i)));
      Object raw_key = properties->KeyAt(key_index);
      DCHECK(properties->IsKey(roots, raw_key));
      DCHECK(raw_key.IsName());
      Handle<Name> key(Name::cast(raw_key), isolate());
      // If the property is already there we skip it.
      if (PropertyAlreadyExists(isolate(), to, key)) continue;
      // Set the property.
      Handle<Object> value =
          Handle<Object>(properties->ValueAt(key_index), isolate());
      DCHECK(!value->IsCell());
      DCHECK(!value->IsTheHole(isolate()));
      PropertyDetails details = properties->DetailsAt(key_index);
      DCHECK_EQ(kData, details.kind());
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  }
}

void Genesis::TransferIndexedProperties(Handle<JSObject> from,
                                        Handle<JSObject> to) {
  // Cloning the elements array is sufficient.
  Handle<FixedArray> from_elements =
      Handle<FixedArray>(FixedArray::cast(from->elements()), isolate());
  Handle<FixedArray> to_elements = factory()->CopyFixedArray(from_elements);
  to->set_elements(*to_elements);
}

void Genesis::TransferObject(Handle<JSObject> from, Handle<JSObject> to) {
  HandleScope outer(isolate());

  DCHECK(!from->IsJSArray());
  DCHECK(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<HeapObject> proto(from->map().prototype(), isolate());
  JSObject::ForceSetPrototype(to, proto);
}

Handle<Map> Genesis::CreateInitialMapForArraySubclass(int size,
                                                      int inobject_properties) {
  // Find global.Array.prototype to inherit from.
  Handle<JSFunction> array_constructor(native_context()->array_function(),
                                       isolate());
  Handle<JSObject> array_prototype(native_context()->initial_array_prototype(),
                                   isolate());

  // Add initial map.
  Handle<Map> initial_map = factory()->NewMap(
      JS_ARRAY_TYPE, size, TERMINAL_FAST_ELEMENTS_KIND, inobject_properties);
  initial_map->SetConstructor(*array_constructor);

  // Set prototype on map.
  initial_map->set_has_non_instance_prototype(false);
  Map::SetPrototype(isolate(), initial_map, array_prototype);

  // Update map with length accessor from Array.
  static constexpr int kTheLengthAccessor = 1;
  Map::EnsureDescriptorSlack(isolate(), initial_map,
                             inobject_properties + kTheLengthAccessor);

  // length descriptor.
  {
    JSFunction array_function = native_context()->array_function();
    Handle<DescriptorArray> array_descriptors(
        array_function.initial_map().instance_descriptors(), isolate());
    Handle<String> length = factory()->length_string();
    InternalIndex old = array_descriptors->SearchWithCache(
        isolate(), *length, array_function.initial_map());
    DCHECK(old.is_found());
    Descriptor d = Descriptor::AccessorConstant(
        length, handle(array_descriptors->GetStrongValue(old), isolate()),
        array_descriptors->GetDetails(old).attributes());
    initial_map->AppendDescriptor(isolate(), &d);
  }
  return initial_map;
}

Genesis::Genesis(
    Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  RuntimeCallTimerScope rcs_timer(isolate, RuntimeCallCounterId::kGenesis);
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  // The deserializer needs to hook up references to the global proxy.
  // Create an uninitialized global proxy now if we don't have one
  // and initialize it later in CreateNewGlobals.
  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    int instance_size = 0;
    if (context_snapshot_index > 0) {
      // The global proxy function to reinitialize this global proxy is in the
      // context that is yet to be deserialized. We need to prepare a global
      // proxy of the correct size.
      Object size = isolate->heap()->serialized_global_proxy_sizes().get(
          static_cast<int>(context_snapshot_index) - 1);
      instance_size = Smi::ToInt(size);
    } else {
      instance_size = JSGlobalProxy::SizeWithEmbedderFields(
          global_proxy_template.IsEmpty()
              ? 0
              : global_proxy_template->InternalFieldCount());
    }
    global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy(instance_size);
  }

  // We can only de-serialize a context if the isolate was initialized from
  // a snapshot. Otherwise we have to build the context from scratch.
  // Also create a context from scratch to expose natives, if required by flag.
  DCHECK(native_context_.is_null());
  if (isolate->initialized_from_snapshot()) {
    Handle<Context> context;
    if (Snapshot::NewContextFromSnapshot(isolate, global_proxy,
                                         context_snapshot_index,
                                         embedder_fields_deserializer)
            .ToHandle(&context)) {
      native_context_ = Handle<NativeContext>::cast(context);
    }
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(isolate, *native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();

    if (context_snapshot_index == 0) {
      Handle<JSGlobalObject> global_object =
          CreateNewGlobals(global_proxy_template, global_proxy);
      HookUpGlobalObject(global_object);

      if (!ConfigureGlobalObjects(global_proxy_template)) return;
    } else {
      // The global proxy needs to be integrated into the native context.
      HookUpGlobalProxy(global_proxy);
    }
    DCHECK(!global_proxy->IsDetachedFrom(native_context()->global_object()));
  } else {
    DCHECK(native_context().is_null());

    base::ElapsedTimer timer;
    if (FLAG_profile_deserialization) timer.Start();
    DCHECK_EQ(0u, context_snapshot_index);
    // We get here if there was no context snapshot.
    CreateRoots();
    MathRandom::InitializeContext(isolate, native_context());
    Handle<JSFunction> empty_function = CreateEmptyFunction();
    CreateSloppyModeFunctionMaps(empty_function);
    CreateStrictModeFunctionMaps(empty_function);
    CreateObjectFunction(empty_function);
    CreateIteratorMaps(empty_function);
    CreateAsyncIteratorMaps(empty_function);
    CreateAsyncFunctionMaps(empty_function);
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);
    InitializeGlobal(global_object, empty_function);
    InitializeNormalizedMapCaches();
    InitializeIteratorFunctions();
    InitializeCallSiteBuiltins();

    if (!InstallABunchOfRandomThings()) return;
    if (!InstallExtrasBindings()) return;
    if (!ConfigureGlobalObjects(global_proxy_template)) return;

    isolate->counters()->contexts_created_from_scratch()->Increment();

    if (FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      PrintF("[Initializing context from scratch took %0.3f ms]\n", ms);
    }
  }

  native_context()->set_microtask_queue(
      microtask_queue ? static_cast<MicrotaskQueue*>(microtask_queue)
                      : isolate->default_microtask_queue());

  // Install experimental natives. Do not include them into the
  // snapshot as we should be able to turn them off at runtime. Re-installing
  // them after they have already been deserialized would also fail.
  if (!isolate->serializer_enabled()) {
    InitializeExperimentalGlobal();

    // Store String.prototype's map again in case it has been changed by
    // experimental natives.
    Handle<JSFunction> string_function(native_context()->string_function(),
                                       isolate);
    JSObject string_function_prototype =
        JSObject::cast(string_function->initial_map().prototype());
    DCHECK(string_function_prototype.HasFastProperties());
    native_context()->set_string_function_prototype_map(
        string_function_prototype.map());
  }

  if (FLAG_disallow_code_generation_from_strings) {
    native_context()->set_allow_code_gen_from_strings(
        ReadOnlyRoots(isolate).false_value());
  }

  // We created new functions, which may require debug instrumentation.
  if (isolate->debug()->is_active()) {
    isolate->debug()->InstallDebugBreakTrampoline();
  }

  native_context()->ResetErrorsThrown();
  result_ = native_context();
}

Genesis::Genesis(Isolate* isolate,
                 MaybeHandle<JSGlobalProxy> maybe_global_proxy,
                 v8::Local<v8::ObjectTemplate> global_proxy_template)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  const int proxy_size = JSGlobalProxy::SizeWithEmbedderFields(
      global_proxy_template->InternalFieldCount());

  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    global_proxy = factory()->NewUninitializedJSGlobalProxy(proxy_size);
  }

  // Create a remote object as the global object.
  Handle<ObjectTemplateInfo> global_proxy_data =
      Utils::OpenHandle(*global_proxy_template);
  Handle<FunctionTemplateInfo> global_constructor(
      FunctionTemplateInfo::cast(global_proxy_data->constructor()), isolate);

  Handle<ObjectTemplateInfo> global_object_template(
      ObjectTemplateInfo::cast(global_constructor->GetPrototypeTemplate()),
      isolate);
  Handle<JSObject> global_object =
      ApiNatives::InstantiateRemoteObject(global_object_template)
          .ToHandleChecked();

  // (Re)initialize the global proxy object.
  DCHECK_EQ(global_proxy_data->embedder_field_count(),
            global_proxy_template->InternalFieldCount());
  Handle<Map> global_proxy_map = isolate->factory()->NewMap(
      JS_GLOBAL_PROXY_TYPE, proxy_size, TERMINAL_FAST_ELEMENTS_KIND);
  global_proxy_map->set_is_access_check_needed(true);
  global_proxy_map->set_may_have_interesting_symbols(true);

  // A remote global proxy has no native context.
  global_proxy->set_native_context(ReadOnlyRoots(heap()).null_value());

  // Configure the hidden prototype chain of the global proxy.
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->map().SetConstructor(*global_constructor);

  global_proxy_ = global_proxy;
}

// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() { return sizeof(NestingCounterType); }

// Archive statics that are thread-local.
char* Bootstrapper::ArchiveState(char* to) {
  *reinterpret_cast<NestingCounterType*>(to) = nesting_;
  nesting_ = 0;
  return to + sizeof(NestingCounterType);
}

// Restore statics that are thread-local.
char* Bootstrapper::RestoreState(char* from) {
  nesting_ = *reinterpret_cast<NestingCounterType*>(from);
  return from + sizeof(NestingCounterType);
}

// Called when the top-level V8 mutex is destroyed.
void Bootstrapper::FreeThreadResources() { DCHECK(!IsActive()); }

}  // namespace internal
}  // namespace v8

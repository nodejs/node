// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"

#include "src/accessors.h"
#include "src/api-natives.h"
#include "src/base/ieee754.h"
#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/free-buffer-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/ignition-statistics-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#include "src/ffi/ffi-compiler.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-js.h"

#if V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION) {}

Handle<String> Bootstrapper::GetNativeSource(NativeType type, int index) {
  NativesExternalStringResource* resource =
      new NativesExternalStringResource(type, index);
  Handle<ExternalOneByteString> source_code =
      isolate_->factory()->NewNativeSourceString(resource);
  isolate_->heap()->RegisterExternalString(*source_code);
  DCHECK(source_code->is_short());
  return source_code;
}

void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(isolate_, create_heap_objects);
}


static const char* GCFunctionName() {
  bool flag_given = FLAG_expose_gc_as != NULL && strlen(FLAG_expose_gc_as) != 0;
  return flag_given ? FLAG_expose_gc_as : "gc";
}


v8::Extension* Bootstrapper::free_buffer_extension_ = NULL;
v8::Extension* Bootstrapper::gc_extension_ = NULL;
v8::Extension* Bootstrapper::externalize_string_extension_ = NULL;
v8::Extension* Bootstrapper::statistics_extension_ = NULL;
v8::Extension* Bootstrapper::trigger_failure_extension_ = NULL;
v8::Extension* Bootstrapper::ignition_statistics_extension_ = NULL;

void Bootstrapper::InitializeOncePerProcess() {
  free_buffer_extension_ = new FreeBufferExtension;
  v8::RegisterExtension(free_buffer_extension_);
  gc_extension_ = new GCExtension(GCFunctionName());
  v8::RegisterExtension(gc_extension_);
  externalize_string_extension_ = new ExternalizeStringExtension;
  v8::RegisterExtension(externalize_string_extension_);
  statistics_extension_ = new StatisticsExtension;
  v8::RegisterExtension(statistics_extension_);
  trigger_failure_extension_ = new TriggerFailureExtension;
  v8::RegisterExtension(trigger_failure_extension_);
  ignition_statistics_extension_ = new IgnitionStatisticsExtension;
  v8::RegisterExtension(ignition_statistics_extension_);
}


void Bootstrapper::TearDownExtensions() {
  delete free_buffer_extension_;
  free_buffer_extension_ = NULL;
  delete gc_extension_;
  gc_extension_ = NULL;
  delete externalize_string_extension_;
  externalize_string_extension_ = NULL;
  delete statistics_extension_;
  statistics_extension_ = NULL;
  delete trigger_failure_extension_;
  trigger_failure_extension_ = NULL;
  delete ignition_statistics_extension_;
  ignition_statistics_extension_ = NULL;
}

void Bootstrapper::TearDown() {
  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}


class Genesis BASE_EMBEDDED {
 public:
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template,
          size_t context_snapshot_index,
          v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
          GlobalContextType context_type);
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template);
  ~Genesis() { }

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Heap* heap() const { return isolate_->heap(); }

  Handle<Context> result() { return result_; }

  Handle<JSGlobalProxy> global_proxy() { return global_proxy_; }

 private:
  Handle<Context> native_context() { return native_context_; }

  // Creates some basic objects. Used for creating a context from scratch.
  void CreateRoots();
  // Creates the empty function.  Used for creating a context from scratch.
  Handle<JSFunction> CreateEmptyFunction(Isolate* isolate);
  // Creates the ThrowTypeError function. ECMA 5th Ed. 13.2.3
  Handle<JSFunction> GetRestrictedFunctionPropertiesThrower();
  Handle<JSFunction> GetStrictArgumentsPoisonFunction();
  Handle<JSFunction> GetThrowTypeErrorIntrinsic(Builtins::Name builtin_name);

  void CreateStrictModeFunctionMaps(Handle<JSFunction> empty);
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
                        Handle<JSFunction> empty_function,
                        GlobalContextType context_type);
  void InitializeExperimentalGlobal();
  // Depending on the situation, expose and/or get rid of the utils object.
  void ConfigureUtilsObject(GlobalContextType context_type);

#define DECLARE_FEATURE_INITIALIZATION(id, descr) \
  void InitializeGlobal_##id();

  HARMONY_INPROGRESS(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_STAGED(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_SHIPPING(DECLARE_FEATURE_INITIALIZATION)
#undef DECLARE_FEATURE_INITIALIZATION

  void InstallOneBuiltinFunction(Handle<Object> prototype, const char* method,
                                 Builtins::Name name);

  Handle<JSFunction> InstallArrayBuffer(Handle<JSObject> target,
                                        const char* name,
                                        Builtins::Name call_byteLength,
                                        BuiltinFunctionId byteLength_id,
                                        Builtins::Name call_slice);
  Handle<JSFunction> InstallInternalArray(Handle<JSObject> target,
                                          const char* name,
                                          ElementsKind elements_kind);
  bool InstallNatives(GlobalContextType context_type);

  void InstallTypedArray(const char* name, ElementsKind elements_kind,
                         Handle<JSFunction>* fun);
  bool InstallExtraNatives();
  bool InstallExperimentalExtraNatives();
  bool InstallDebuggerNatives();
  void InstallBuiltinFunctionIds();
  void InstallExperimentalBuiltinFunctionIds();
  void InitializeNormalizedMapCaches();

  enum ExtensionTraversalState {
    UNVISITED, VISITED, INSTALLED
  };

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
  static bool InstallExtensions(Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states);
  static bool InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static bool InstallSpecialObjects(Handle<Context> native_context);
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

  void MakeFunctionInstancePrototypeWritable();

  void SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                           FunctionMode function_mode);

  static bool CallUtilsFunction(Isolate* isolate, const char* name);

  static bool CompileExtension(Isolate* isolate, v8::Extension* extension);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<Context> native_context_;
  Handle<JSGlobalProxy> global_proxy_;

  // Function maps. Function maps are created initially with a read only
  // prototype for the processing of JS builtins. Later the function maps are
  // replaced in order to make prototype writable. These are the final, writable
  // prototype, maps.
  Handle<Map> sloppy_function_map_writable_prototype_;
  Handle<Map> strict_function_map_writable_prototype_;
  Handle<Map> class_function_map_;
  Handle<JSFunction> strict_poison_function_;
  Handle<JSFunction> restricted_function_properties_thrower_;

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
    GlobalContextType context_type) {
  HandleScope scope(isolate_);
  Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,
                  context_snapshot_index, embedder_fields_deserializer,
                  context_type);
  Handle<Context> env = genesis.result();
  if (env.is_null() || !InstallExtensions(env, extensions)) {
    return Handle<Context>();
  }
  return scope.CloseAndEscape(env);
}

Handle<JSGlobalProxy> Bootstrapper::NewRemoteContext(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  HandleScope scope(isolate_);
  Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template);
  Handle<JSGlobalProxy> global_proxy = genesis.global_proxy();
  if (global_proxy.is_null()) return Handle<JSGlobalProxy>();
  return scope.CloseAndEscape(global_proxy);
}

void Bootstrapper::DetachGlobal(Handle<Context> env) {
  Isolate* isolate = env->GetIsolate();
  isolate->counters()->errors_thrown_per_context()->AddSample(
      env->GetErrorsThrown());

  Heap* heap = isolate->heap();
  Handle<JSGlobalProxy> global_proxy(JSGlobalProxy::cast(env->global_proxy()));
  global_proxy->set_native_context(heap->null_value());
  JSObject::ForceSetPrototype(global_proxy, isolate->factory()->null_value());
  global_proxy->map()->SetConstructor(heap->null_value());
  if (FLAG_track_detached_contexts) {
    env->GetIsolate()->AddDetachedContext(env);
  }
}

namespace {

void InstallFunction(Handle<JSObject> target, Handle<Name> property_name,
                     Handle<JSFunction> function, Handle<String> function_name,
                     PropertyAttributes attributes = DONT_ENUM) {
  JSObject::AddProperty(target, property_name, function, attributes);
  if (target->IsJSGlobalObject()) {
    function->shared()->set_instance_class_name(*function_name);
  }
}

void InstallFunction(Handle<JSObject> target, Handle<JSFunction> function,
                     Handle<Name> name,
                     PropertyAttributes attributes = DONT_ENUM) {
  Handle<String> name_string = Name::ToFunctionName(name).ToHandleChecked();
  InstallFunction(target, name, function, name_string, attributes);
}

Handle<JSFunction> CreateFunction(Isolate* isolate, Handle<String> name,
                                  InstanceType type, int instance_size,
                                  MaybeHandle<JSObject> maybe_prototype,
                                  Builtins::Name call,
                                  bool strict_function_map = false) {
  Factory* factory = isolate->factory();
  Handle<Code> call_code(isolate->builtins()->builtin(call));
  Handle<JSObject> prototype;
  Handle<JSFunction> result =
      maybe_prototype.ToHandle(&prototype)
          ? factory->NewFunction(name, call_code, prototype, type,
                                 instance_size, strict_function_map)
          : factory->NewFunctionWithoutPrototype(name, call_code,
                                                 strict_function_map);
  result->shared()->set_native(true);
  return result;
}

Handle<JSFunction> InstallFunction(Handle<JSObject> target, Handle<Name> name,
                                   InstanceType type, int instance_size,
                                   MaybeHandle<JSObject> maybe_prototype,
                                   Builtins::Name call,
                                   PropertyAttributes attributes,
                                   bool strict_function_map = false) {
  Handle<String> name_string = Name::ToFunctionName(name).ToHandleChecked();
  Handle<JSFunction> function =
      CreateFunction(target->GetIsolate(), name_string, type, instance_size,
                     maybe_prototype, call, strict_function_map);
  InstallFunction(target, name, function, name_string, attributes);
  return function;
}

Handle<JSFunction> InstallFunction(Handle<JSObject> target, const char* name,
                                   InstanceType type, int instance_size,
                                   MaybeHandle<JSObject> maybe_prototype,
                                   Builtins::Name call,
                                   bool strict_function_map = false) {
  Factory* const factory = target->GetIsolate()->factory();
  PropertyAttributes attributes = DONT_ENUM;
  return InstallFunction(target, factory->InternalizeUtf8String(name), type,
                         instance_size, maybe_prototype, call, attributes,
                         strict_function_map);
}

Handle<JSFunction> SimpleCreateFunction(Isolate* isolate, Handle<String> name,
                                        Builtins::Name call, int len,
                                        bool adapt) {
  Handle<JSFunction> fun =
      CreateFunction(isolate, name, JS_OBJECT_TYPE, JSObject::kHeaderSize,
                     MaybeHandle<JSObject>(), call);
  if (adapt) {
    fun->shared()->set_internal_formal_parameter_count(len);
  } else {
    fun->shared()->DontAdaptArguments();
  }
  fun->shared()->set_length(len);
  return fun;
}

Handle<JSFunction> InstallArrayBuiltinFunction(Handle<JSObject> base,
                                               const char* name,
                                               Builtins::Name call) {
  Isolate* isolate = base->GetIsolate();
  Handle<String> str_name = isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun =
      CreateFunction(isolate, str_name, JS_OBJECT_TYPE, JSObject::kHeaderSize,
                     MaybeHandle<JSObject>(), call, true);
  fun->shared()->set_internal_formal_parameter_count(
      Builtins::GetBuiltinParameterCount(call));

  // Set the length to 1 to satisfy ECMA-262.
  fun->shared()->set_length(1);
  fun->shared()->set_language_mode(STRICT);
  InstallFunction(base, fun, str_name);
  return fun;
}

Handle<JSFunction> SimpleInstallFunction(Handle<JSObject> base,
                                         Handle<String> name,
                                         Builtins::Name call, int len,
                                         bool adapt,
                                         PropertyAttributes attrs = DONT_ENUM) {
  Handle<JSFunction> fun =
      SimpleCreateFunction(base->GetIsolate(), name, call, len, adapt);
  InstallFunction(base, fun, name, attrs);
  return fun;
}

Handle<JSFunction> SimpleInstallFunction(Handle<JSObject> base,
                                         const char* name, Builtins::Name call,
                                         int len, bool adapt,
                                         PropertyAttributes attrs = DONT_ENUM) {
  Factory* const factory = base->GetIsolate()->factory();
  return SimpleInstallFunction(base, factory->InternalizeUtf8String(name), call,
                               len, adapt, attrs);
}

Handle<JSFunction> SimpleInstallFunction(Handle<JSObject> base,
                                         const char* name, Builtins::Name call,
                                         int len, bool adapt,
                                         BuiltinFunctionId id) {
  Handle<JSFunction> fun = SimpleInstallFunction(base, name, call, len, adapt);
  fun->shared()->set_builtin_function_id(id);
  return fun;
}

void SimpleInstallGetterSetter(Handle<JSObject> base, Handle<String> name,
                               Builtins::Name call_getter,
                               Builtins::Name call_setter,
                               PropertyAttributes attribs) {
  Isolate* const isolate = base->GetIsolate();

  Handle<String> getter_name =
      Name::ToFunctionName(name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call_getter, 0, true);

  Handle<String> setter_name =
      Name::ToFunctionName(name, isolate->factory()->set_string())
          .ToHandleChecked();
  Handle<JSFunction> setter =
      SimpleCreateFunction(isolate, setter_name, call_setter, 1, true);

  JSObject::DefineAccessor(base, name, getter, setter, attribs).Check();
}

Handle<JSFunction> SimpleInstallGetter(Handle<JSObject> base,
                                       Handle<String> name,
                                       Handle<Name> property_name,
                                       Builtins::Name call, bool adapt) {
  Isolate* const isolate = base->GetIsolate();

  Handle<String> getter_name =
      Name::ToFunctionName(name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call, 0, adapt);

  Handle<Object> setter = isolate->factory()->undefined_value();

  JSObject::DefineAccessor(base, property_name, getter, setter, DONT_ENUM)
      .Check();

  return getter;
}

Handle<JSFunction> SimpleInstallGetter(Handle<JSObject> base,
                                       Handle<String> name, Builtins::Name call,
                                       bool adapt) {
  return SimpleInstallGetter(base, name, name, call, adapt);
}

Handle<JSFunction> SimpleInstallGetter(Handle<JSObject> base,
                                       Handle<String> name, Builtins::Name call,
                                       bool adapt, BuiltinFunctionId id) {
  Handle<JSFunction> fun = SimpleInstallGetter(base, name, call, adapt);
  fun->shared()->set_builtin_function_id(id);
  return fun;
}

void InstallConstant(Isolate* isolate, Handle<JSObject> holder,
                     const char* name, Handle<Object> value) {
  JSObject::AddProperty(
      holder, isolate->factory()->NewStringFromAsciiChecked(name), value,
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
}

void InstallSpeciesGetter(Handle<JSFunction> constructor) {
  Factory* factory = constructor->GetIsolate()->factory();
  // TODO(adamk): We should be able to share a SharedFunctionInfo
  // between all these JSFunctins.
  SimpleInstallGetter(constructor, factory->symbol_species_string(),
                      factory->species_symbol(), Builtins::kReturnReceiver,
                      true);
}

}  // namespace

Handle<JSFunction> Genesis::CreateEmptyFunction(Isolate* isolate) {
  // Allocate the map for function instances. Maps are allocated first and their
  // prototypes patched later, once empty function is created.

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      factory()->CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  native_context()->set_sloppy_function_without_prototype_map(
      *function_without_prototype_map);

  // Allocate the function map. This map is temporary, used only for processing
  // of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> function_map =
      factory()->CreateSloppyFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE);
  native_context()->set_sloppy_function_map(*function_map);
  native_context()->set_sloppy_function_with_readonly_prototype_map(
      *function_map);

  // The final map for functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  sloppy_function_map_writable_prototype_ =
      factory()->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE);
  Factory* factory = isolate->factory();

  Handle<String> object_name = factory->Object_string();

  Handle<JSObject> object_function_prototype;

  {  // --- O b j e c t ---
    Handle<JSFunction> object_fun = factory->NewFunction(object_name);
    int unused = JSObject::kInitialGlobalObjectUnusedPropertiesCount;
    int instance_size = JSObject::kHeaderSize + kPointerSize * unused;
    Handle<Map> object_function_map =
        factory->NewMap(JS_OBJECT_TYPE, instance_size);
    object_function_map->SetInObjectProperties(unused);
    JSFunction::SetInitialMap(object_fun, object_function_map,
                              isolate->factory()->null_value());
    object_function_map->set_unused_property_fields(unused);

    native_context()->set_object_function(*object_fun);

    // Allocate a new prototype for the object function.
    object_function_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<Map> map = Map::Copy(handle(object_function_prototype->map()),
                                "EmptyObjectPrototype");
    map->set_is_prototype_map(true);
    // Ban re-setting Object.prototype.__proto__ to prevent Proxy security bug
    map->set_immutable_proto(true);
    object_function_prototype->set_map(*map);

    native_context()->set_initial_object_prototype(*object_function_prototype);
    JSFunction::SetPrototype(object_fun, object_function_prototype);

    {
      // Set up slow map for Object.create(null) instances without in-object
      // properties.
      Handle<Map> map(object_fun->initial_map(), isolate);
      map = Map::CopyInitialMapNormalized(map);
      Map::SetPrototype(map, isolate->factory()->null_value());
      native_context()->set_slow_object_with_null_prototype_map(*map);

      // Set up slow map for literals with too many properties.
      map = Map::Copy(map, "slow_object_with_object_prototype_map");
      Map::SetPrototype(map, object_function_prototype);
      native_context()->set_slow_object_with_object_prototype_map(*map);
    }
  }

  // Allocate the empty function as the prototype for function - ES6 19.2.3
  Handle<Code> code(isolate->builtins()->EmptyFunction());
  Handle<JSFunction> empty_function =
      factory->NewFunctionWithoutPrototype(factory->empty_string(), code);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_function_map =
      factory->CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  DCHECK(!empty_function_map->is_dictionary_map());
  Map::SetPrototype(empty_function_map, object_function_prototype);
  empty_function_map->set_is_prototype_map(true);

  empty_function->set_map(*empty_function_map);

  // --- E m p t y ---
  Handle<String> source = factory->NewStringFromStaticChars("() {}");
  Handle<Script> script = factory->NewScript(source);
  script->set_type(Script::TYPE_NATIVE);
  Handle<FixedArray> infos = factory->NewFixedArray(2);
  script->set_shared_function_infos(*infos);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
  empty_function->shared()->set_function_literal_id(1);
  empty_function->shared()->DontAdaptArguments();
  SharedFunctionInfo::SetScript(handle(empty_function->shared()), script);

  // Set prototypes for the function maps.
  Handle<Map> sloppy_function_map(native_context()->sloppy_function_map(),
                                  isolate);
  Handle<Map> sloppy_function_without_prototype_map(
      native_context()->sloppy_function_without_prototype_map(), isolate);
  Map::SetPrototype(sloppy_function_map, empty_function);
  Map::SetPrototype(sloppy_function_without_prototype_map, empty_function);
  Map::SetPrototype(sloppy_function_map_writable_prototype_, empty_function);

  return empty_function;
}


// Creates the %ThrowTypeError% function.
Handle<JSFunction> Genesis::GetThrowTypeErrorIntrinsic(
    Builtins::Name builtin_name) {
  Handle<String> name =
      factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("ThrowTypeError"));
  Handle<Code> code(isolate()->builtins()->builtin(builtin_name));
  Handle<JSFunction> function =
      factory()->NewFunctionWithoutPrototype(name, code, true);
  function->shared()->DontAdaptArguments();

  // %ThrowTypeError% must not have a name property.
  if (JSReceiver::DeleteProperty(function, factory()->name_string())
          .IsNothing()) {
    DCHECK(false);
  }

  // length needs to be non configurable.
  Handle<Object> value(Smi::FromInt(function->shared()->GetLength()),
                       isolate());
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->length_string(), value,
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  if (JSObject::PreventExtensions(function, Object::THROW_ON_ERROR)
          .IsNothing()) {
    DCHECK(false);
  }

  JSObject::MigrateSlowToFast(function, 0, "Bootstrapping");

  return function;
}


// ECMAScript 5th Edition, 13.2.3
Handle<JSFunction> Genesis::GetRestrictedFunctionPropertiesThrower() {
  if (restricted_function_properties_thrower_.is_null()) {
    restricted_function_properties_thrower_ = GetThrowTypeErrorIntrinsic(
        Builtins::kRestrictedFunctionPropertiesThrower);
  }
  return restricted_function_properties_thrower_;
}


Handle<JSFunction> Genesis::GetStrictArgumentsPoisonFunction() {
  if (strict_poison_function_.is_null()) {
    strict_poison_function_ = GetThrowTypeErrorIntrinsic(
        Builtins::kRestrictedStrictArgumentsPropertiesThrower);
  }
  return strict_poison_function_;
}


void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  // Allocate map for the prototype-less strict mode instances.
  Handle<Map> strict_function_without_prototype_map =
      factory()->CreateStrictFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_strict_function_without_prototype_map(
      *strict_function_without_prototype_map);

  // Allocate map for the strict mode functions. This map is temporary, used
  // only for processing of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> strict_function_map = factory()->CreateStrictFunctionMap(
      FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_function_map(*strict_function_map);

  // The final map for the strict mode functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  strict_function_map_writable_prototype_ = factory()->CreateStrictFunctionMap(
      FUNCTION_WITH_WRITEABLE_PROTOTYPE, empty);

  // Allocate map for classes
  class_function_map_ = factory()->CreateClassFunctionMap(empty);
  native_context()->set_class_function_map(*class_function_map_);

  // Now that the strict mode function map is available, set up the
  // restricted "arguments" and "caller" getters.
  AddRestrictedFunctionProperties(empty);
}

void Genesis::CreateIteratorMaps(Handle<JSFunction> empty) {
  // Create iterator-related meta-objects.
  Handle<JSObject> iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  Handle<JSFunction> iterator_prototype_iterator = SimpleCreateFunction(
      isolate(), factory()->NewStringFromAsciiChecked("[Symbol.iterator]"),
      Builtins::kReturnReceiver, 0, true);

  JSObject::AddProperty(iterator_prototype, factory()->iterator_symbol(),
                        iterator_prototype_iterator, DONT_ENUM);
  native_context()->set_initial_iterator_prototype(*iterator_prototype);

  Handle<JSObject> generator_object_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  native_context()->set_initial_generator_prototype(
      *generator_object_prototype);
  JSObject::ForceSetPrototype(generator_object_prototype, iterator_prototype);
  Handle<JSObject> generator_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::ForceSetPrototype(generator_function_prototype, empty);

  JSObject::AddProperty(
      generator_function_prototype, factory()->to_string_tag_symbol(),
      factory()->NewStringFromAsciiChecked("GeneratorFunction"),
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(generator_function_prototype,
                        factory()->prototype_string(),
                        generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::AddProperty(generator_object_prototype,
                        factory()->constructor_string(),
                        generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(generator_object_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->NewStringFromAsciiChecked("Generator"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  SimpleInstallFunction(generator_object_prototype, "next",
                        Builtins::kGeneratorPrototypeNext, 1, true);
  SimpleInstallFunction(generator_object_prototype, "return",
                        Builtins::kGeneratorPrototypeReturn, 1, true);
  SimpleInstallFunction(generator_object_prototype, "throw",
                        Builtins::kGeneratorPrototypeThrow, 1, true);

  // Internal version of generator_prototype_next, flagged as non-native such
  // that it doesn't show up in Error traces.
  Handle<JSFunction> generator_next_internal =
      SimpleCreateFunction(isolate(), factory()->next_string(),
                           Builtins::kGeneratorPrototypeNext, 1, true);
  generator_next_internal->shared()->set_native(false);
  native_context()->set_generator_next_internal(*generator_next_internal);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  Handle<Map> strict_function_map(strict_function_map_writable_prototype_);
  // Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> generator_function_map =
      Map::Copy(strict_function_map, "GeneratorFunction");
  generator_function_map->set_is_constructor(false);
  Map::SetPrototype(generator_function_map, generator_function_prototype);
  native_context()->set_generator_function_map(*generator_function_map);

  Handle<JSFunction> object_function(native_context()->object_function());
  Handle<Map> generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(generator_object_prototype_map, generator_object_prototype);
  native_context()->set_generator_object_prototype_map(
      *generator_object_prototype_map);
}

void Genesis::CreateAsyncIteratorMaps(Handle<JSFunction> empty) {
  // %AsyncIteratorPrototype%
  // proposal-async-iteration/#sec-asynciteratorprototype
  Handle<JSObject> async_iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  Handle<JSFunction> async_iterator_prototype_iterator = SimpleCreateFunction(
      isolate(), factory()->NewStringFromAsciiChecked("[Symbol.asyncIterator]"),
      Builtins::kReturnReceiver, 0, true);

  JSObject::AddProperty(async_iterator_prototype,
                        factory()->async_iterator_symbol(),
                        async_iterator_prototype_iterator, DONT_ENUM);

  // %AsyncFromSyncIteratorPrototype%
  // proposal-async-iteration/#sec-%asyncfromsynciteratorprototype%-object
  Handle<JSObject> async_from_sync_iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  SimpleInstallFunction(async_from_sync_iterator_prototype,
                        factory()->next_string(),
                        Builtins::kAsyncFromSyncIteratorPrototypeNext, 1, true);
  SimpleInstallFunction(
      async_from_sync_iterator_prototype, factory()->return_string(),
      Builtins::kAsyncFromSyncIteratorPrototypeReturn, 1, true);
  SimpleInstallFunction(
      async_from_sync_iterator_prototype, factory()->throw_string(),
      Builtins::kAsyncFromSyncIteratorPrototypeThrow, 1, true);

  JSObject::AddProperty(
      async_from_sync_iterator_prototype, factory()->to_string_tag_symbol(),
      factory()->NewStringFromAsciiChecked("Async-from-Sync Iterator"),
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::ForceSetPrototype(async_from_sync_iterator_prototype,
                              async_iterator_prototype);

  Handle<Map> async_from_sync_iterator_map = factory()->NewMap(
      JS_ASYNC_FROM_SYNC_ITERATOR_TYPE, JSAsyncFromSyncIterator::kSize);
  Map::SetPrototype(async_from_sync_iterator_map,
                    async_from_sync_iterator_prototype);
  native_context()->set_async_from_sync_iterator_map(
      *async_from_sync_iterator_map);

  // Async Generators
  Handle<String> AsyncGeneratorFunction_string =
      factory()->NewStringFromAsciiChecked("AsyncGeneratorFunction", TENURED);

  Handle<JSObject> async_generator_object_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSObject> async_generator_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  // %AsyncGenerator% / %AsyncGeneratorFunction%.prototype
  JSObject::ForceSetPrototype(async_generator_function_prototype, empty);

  // The value of AsyncGeneratorFunction.prototype.prototype is the
  //     %AsyncGeneratorPrototype% intrinsic object.
  // This property has the attributes
  //     { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
  JSObject::AddProperty(async_generator_function_prototype,
                        factory()->prototype_string(),
                        async_generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(async_generator_function_prototype,
                        factory()->to_string_tag_symbol(),
                        AsyncGeneratorFunction_string,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // %AsyncGeneratorPrototype%
  JSObject::ForceSetPrototype(async_generator_object_prototype,
                              async_iterator_prototype);

  JSObject::AddProperty(async_generator_object_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->NewStringFromAsciiChecked("AsyncGenerator"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  SimpleInstallFunction(async_generator_object_prototype, "next",
                        Builtins::kAsyncGeneratorPrototypeNext, 1, true);
  SimpleInstallFunction(async_generator_object_prototype, "return",
                        Builtins::kAsyncGeneratorPrototypeReturn, 1, true);
  SimpleInstallFunction(async_generator_object_prototype, "throw",
                        Builtins::kAsyncGeneratorPrototypeThrow, 1, true);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  Handle<Map> strict_function_map(strict_function_map_writable_prototype_);
  // Async Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> async_generator_function_map =
      Map::Copy(strict_function_map, "AsyncGeneratorFunction");
  async_generator_function_map->set_is_constructor(false);
  Map::SetPrototype(async_generator_function_map,
                    async_generator_function_prototype);
  native_context()->set_async_generator_function_map(
      *async_generator_function_map);

  Handle<JSFunction> object_function(native_context()->object_function());
  Handle<Map> async_generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(async_generator_object_prototype_map,
                    async_generator_object_prototype);
  native_context()->set_async_generator_object_prototype_map(
      *async_generator_object_prototype_map);
}

void Genesis::CreateAsyncFunctionMaps(Handle<JSFunction> empty) {
  // %AsyncFunctionPrototype% intrinsic
  Handle<JSObject> async_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::ForceSetPrototype(async_function_prototype, empty);

  JSObject::AddProperty(async_function_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->NewStringFromAsciiChecked("AsyncFunction"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  Handle<Map> strict_function_map(
      native_context()->strict_function_without_prototype_map());
  Handle<Map> async_function_map =
      Map::Copy(strict_function_map, "AsyncFunction");
  async_function_map->set_is_constructor(false);
  Map::SetPrototype(async_function_map, async_function_prototype);
  native_context()->set_async_function_map(*async_function_map);
}

void Genesis::CreateJSProxyMaps() {
  // Allocate the different maps for all Proxy types.
  // Next to the default proxy, we need maps indicating callable and
  // constructable proxies.
  Handle<Map> proxy_function_map =
      Map::Copy(isolate()->sloppy_function_without_prototype_map(), "Proxy");
  proxy_function_map->set_is_constructor(true);
  native_context()->set_proxy_function_map(*proxy_function_map);

  Handle<Map> proxy_map =
      factory()->NewMap(JS_PROXY_TYPE, JSProxy::kSize, FAST_ELEMENTS);
  proxy_map->set_dictionary_map(true);
  native_context()->set_proxy_map(*proxy_map);

  Handle<Map> proxy_callable_map = Map::Copy(proxy_map, "callable Proxy");
  proxy_callable_map->set_is_callable();
  native_context()->set_proxy_callable_map(*proxy_callable_map);
  proxy_callable_map->SetConstructor(native_context()->function_function());

  Handle<Map> proxy_constructor_map =
      Map::Copy(proxy_callable_map, "constructor Proxy");
  proxy_constructor_map->set_is_constructor(true);
  native_context()->set_proxy_constructor_map(*proxy_constructor_map);
}

static void ReplaceAccessors(Handle<Map> map,
                             Handle<String> name,
                             PropertyAttributes attributes,
                             Handle<AccessorPair> accessor_pair) {
  DescriptorArray* descriptors = map->instance_descriptors();
  int idx = descriptors->SearchWithCache(map->GetIsolate(), *name, *map);
  Descriptor d = Descriptor::AccessorConstant(name, accessor_pair, attributes);
  descriptors->Replace(idx, &d);
}

void Genesis::AddRestrictedFunctionProperties(Handle<JSFunction> empty) {
  PropertyAttributes rw_attribs = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> thrower = GetRestrictedFunctionPropertiesThrower();
  Handle<AccessorPair> accessors = factory()->NewAccessorPair();
  accessors->set_getter(*thrower);
  accessors->set_setter(*thrower);

  Handle<Map> map(empty->map());
  ReplaceAccessors(map, factory()->arguments_string(), rw_attribs, accessors);
  ReplaceAccessors(map, factory()->caller_string(), rw_attribs, accessors);
}


static void AddToWeakNativeContextList(Context* context) {
  DCHECK(context->IsNativeContext());
  Isolate* isolate = context->GetIsolate();
  Heap* heap = isolate->heap();
#ifdef DEBUG
  { // NOLINT
    DCHECK(context->next_context_link()->IsUndefined(isolate));
    // Check that context is not in the list yet.
    for (Object* current = heap->native_contexts_list();
         !current->IsUndefined(isolate);
         current = Context::cast(current)->next_context_link()) {
      DCHECK(current != context);
    }
  }
#endif
  context->set(Context::NEXT_CONTEXT_LINK, heap->native_contexts_list(),
               UPDATE_WEAK_WRITE_BARRIER);
  heap->set_native_contexts_list(context);
}


void Genesis::CreateRoots() {
  // Allocate the native context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // native context).
  native_context_ = factory()->NewNativeContext();
  AddToWeakNativeContextList(*native_context());
  isolate()->set_context(*native_context());

  // Allocate the message listeners object.
  {
    Handle<TemplateList> list = TemplateList::New(isolate(), 1);
    native_context()->set_message_listeners(*list);
  }
}


void Genesis::InstallGlobalThisBinding() {
  Handle<ScriptContextTable> script_contexts(
      native_context()->script_context_table());
  Handle<ScopeInfo> scope_info = ScopeInfo::CreateGlobalThisBinding(isolate());
  Handle<JSFunction> closure(native_context()->closure());
  Handle<Context> context = factory()->NewScriptContext(closure, scope_info);

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
            FunctionTemplateInfo::cast(data->constructor()));
    Handle<Object> proto_template(global_constructor->prototype_template(),
                                  isolate());
    if (!proto_template->IsUndefined(isolate())) {
      js_global_object_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_object_template.is_null()) {
    Handle<String> name = Handle<String>(heap()->empty_string());
    Handle<Code> code = isolate()->builtins()->Illegal();
    Handle<JSObject> prototype =
        factory()->NewFunctionPrototype(isolate()->object_function());
    js_global_object_function = factory()->NewFunction(
        name, code, prototype, JS_GLOBAL_OBJECT_TYPE, JSGlobalObject::kSize);
#ifdef DEBUG
    LookupIterator it(prototype, factory()->constructor_string(),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Handle<Object> value = Object::GetProperty(&it).ToHandleChecked();
    DCHECK(it.IsFound());
    DCHECK_EQ(*isolate()->object_function(), *value);
#endif
  } else {
    Handle<FunctionTemplateInfo> js_global_object_constructor(
        FunctionTemplateInfo::cast(js_global_object_template->constructor()));
    js_global_object_function = ApiNatives::CreateApiFunction(
        isolate(), js_global_object_constructor, factory()->the_hole_value(),
        ApiNatives::GlobalObjectType);
  }

  js_global_object_function->initial_map()->set_is_prototype_map(true);
  js_global_object_function->initial_map()->set_dictionary_map(true);
  Handle<JSGlobalObject> global_object =
      factory()->NewJSGlobalObject(js_global_object_function);

  // Step 2: (re)initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_proxy_template.IsEmpty()) {
    Handle<String> name = Handle<String>(heap()->empty_string());
    Handle<Code> code = isolate()->builtins()->Illegal();
    global_proxy_function =
        factory()->NewFunction(name, code, JS_GLOBAL_PROXY_TYPE,
                               JSGlobalProxy::SizeWithEmbedderFields(0));
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor(
            FunctionTemplateInfo::cast(data->constructor()));
    global_proxy_function = ApiNatives::CreateApiFunction(
        isolate(), global_constructor, factory()->the_hole_value(),
        ApiNatives::GlobalProxyType);
  }
  Handle<String> global_name = factory()->global_string();
  global_proxy_function->shared()->set_instance_class_name(*global_name);
  global_proxy_function->initial_map()->set_is_access_check_needed(true);
  global_proxy_function->initial_map()->set_has_hidden_prototype(true);
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
             ->IsUndefined(isolate()) ||
         native_context()->global_proxy() == *global_proxy);
  native_context()->set_global_proxy(*global_proxy);

  return global_object;
}

void Genesis::HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy) {
  // Re-initialize the global proxy with the global proxy function from the
  // snapshot, and then set up the link to the native context.
  Handle<JSFunction> global_proxy_function(
      native_context()->global_proxy_function());
  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()));
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->set_native_context(*native_context());
  DCHECK(native_context()->global_proxy() == *global_proxy);
}

void Genesis::HookUpGlobalObject(Handle<JSGlobalObject> global_object) {
  Handle<JSGlobalObject> global_object_from_snapshot(
      JSGlobalObject::cast(native_context()->extension()));
  native_context()->set_extension(*global_object);
  native_context()->set_security_token(*global_object);

  TransferNamedProperties(global_object_from_snapshot, global_object);
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}

static void InstallWithIntrinsicDefaultProto(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             int context_index) {
  Handle<Smi> index(Smi::FromInt(context_index), isolate);
  JSObject::AddProperty(
      function, isolate->factory()->native_context_index_symbol(), index, NONE);
  isolate->native_context()->set(context_index, *function);
}

static void InstallError(Isolate* isolate, Handle<JSObject> global,
                         Handle<String> name, int context_index) {
  Factory* factory = isolate->factory();

  Handle<JSFunction> error_fun =
      InstallFunction(global, name, JS_ERROR_TYPE, JSObject::kHeaderSize,
                      isolate->initial_object_prototype(),
                      Builtins::kErrorConstructor, DONT_ENUM);
  error_fun->shared()->set_instance_class_name(*factory->Error_string());
  error_fun->shared()->DontAdaptArguments();
  error_fun->shared()->set_construct_stub(
      *isolate->builtins()->ErrorConstructor());
  error_fun->shared()->set_length(1);

  if (context_index == Context::ERROR_FUNCTION_INDEX) {
    SimpleInstallFunction(error_fun, "captureStackTrace",
                          Builtins::kErrorCaptureStackTrace, 2, false);
  }

  InstallWithIntrinsicDefaultProto(isolate, error_fun, context_index);

  {
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);

    JSObject::AddProperty(prototype, factory->name_string(), name, DONT_ENUM);
    JSObject::AddProperty(prototype, factory->message_string(),
                          factory->empty_string(), DONT_ENUM);
    JSObject::AddProperty(prototype, factory->constructor_string(), error_fun,
                          DONT_ENUM);

    if (context_index == Context::ERROR_FUNCTION_INDEX) {
      Handle<JSFunction> to_string_fun =
          SimpleInstallFunction(prototype, factory->toString_string(),
                                Builtins::kErrorPrototypeToString, 0, true);
      isolate->native_context()->set_error_to_string(*to_string_fun);
    } else {
      DCHECK(isolate->native_context()->error_to_string()->IsJSFunction());

      InstallFunction(prototype, isolate->error_to_string(),
                      factory->toString_string(), DONT_ENUM);

      Handle<JSFunction> global_error = isolate->error_function();
      CHECK(JSReceiver::SetPrototype(error_fun, global_error, false,
                                     Object::THROW_ON_ERROR)
                .FromMaybe(false));
      CHECK(JSReceiver::SetPrototype(prototype,
                                     handle(global_error->prototype(), isolate),
                                     false, Object::THROW_ON_ERROR)
                .FromMaybe(false));
    }

    JSFunction::SetPrototype(error_fun, prototype);
  }

  Handle<Map> initial_map(error_fun->initial_map());
  Map::EnsureDescriptorSlack(initial_map, 1);

  PropertyAttributes attribs = DONT_ENUM;
  Handle<AccessorInfo> error_stack =
      Accessors::ErrorStackInfo(isolate, attribs);
  {
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(error_stack->name())), error_stack, attribs);
    initial_map->AppendDescriptor(&d);
  }
}

static void InstallMakeError(Isolate* isolate, Handle<Code> code,
                             int context_index) {
  Handle<JSFunction> function =
      isolate->factory()->NewFunction(isolate->factory()->empty_string(), code,
                                      JS_OBJECT_TYPE, JSObject::kHeaderSize);
  function->shared()->DontAdaptArguments();
  isolate->native_context()->set(context_index, *function);
}

// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpGlobalObject.
void Genesis::InitializeGlobal(Handle<JSGlobalObject> global_object,
                               Handle<JSFunction> empty_function,
                               GlobalContextType context_type) {
  // --- N a t i v e   C o n t e x t ---
  // Use the empty function as closure (no scope info).
  native_context()->set_closure(*empty_function);
  native_context()->set_previous(NULL);
  // Set extension and global object.
  native_context()->set_extension(*global_object);
  // Security setup: Set the security token of the native context to the global
  // object. This makes the security check between two different contexts fail
  // by default even in case of global object reinitialization.
  native_context()->set_security_token(*global_object);

  Isolate* isolate = global_object->GetIsolate();
  Factory* factory = isolate->factory();

  native_context()->set_osr_code_table(*factory->empty_fixed_array());

  Handle<ScriptContextTable> script_context_table =
      factory->NewScriptContextTable();
  native_context()->set_script_context_table(*script_context_table);
  InstallGlobalThisBinding();

  {  // --- O b j e c t ---
    Handle<String> object_name = factory->Object_string();
    Handle<JSFunction> object_function = isolate->object_function();
    JSObject::AddProperty(global_object, object_name, object_function,
                          DONT_ENUM);

    SimpleInstallFunction(object_function, factory->assign_string(),
                          Builtins::kObjectAssign, 2, false);
    SimpleInstallFunction(object_function, "getOwnPropertyDescriptor",
                          Builtins::kObjectGetOwnPropertyDescriptor, 2, false);
    SimpleInstallFunction(object_function,
                          factory->getOwnPropertyDescriptors_string(),
                          Builtins::kObjectGetOwnPropertyDescriptors, 1, false);
    SimpleInstallFunction(object_function, "getOwnPropertyNames",
                          Builtins::kObjectGetOwnPropertyNames, 1, false);
    SimpleInstallFunction(object_function, "getOwnPropertySymbols",
                          Builtins::kObjectGetOwnPropertySymbols, 1, false);
    SimpleInstallFunction(object_function, "is",
                          Builtins::kObjectIs, 2, true);
    SimpleInstallFunction(object_function, "preventExtensions",
                          Builtins::kObjectPreventExtensions, 1, false);
    SimpleInstallFunction(object_function, "seal",
                          Builtins::kObjectSeal, 1, false);

    Handle<JSFunction> object_create =
        SimpleInstallFunction(object_function, factory->create_string(),
                              Builtins::kObjectCreate, 2, true);
    native_context()->set_object_create(*object_create);

    Handle<JSFunction> object_define_properties = SimpleInstallFunction(
        object_function, "defineProperties",
        Builtins::kObjectDefineProperties, 2, true);
    native_context()->set_object_define_properties(*object_define_properties);

    Handle<JSFunction> object_define_property = SimpleInstallFunction(
        object_function, factory->defineProperty_string(),
        Builtins::kObjectDefineProperty, 3, true);
    native_context()->set_object_define_property(*object_define_property);

    Handle<JSFunction> object_freeze = SimpleInstallFunction(
        object_function, "freeze", Builtins::kObjectFreeze, 1, false);
    native_context()->set_object_freeze(*object_freeze);

    Handle<JSFunction> object_get_prototype_of = SimpleInstallFunction(
        object_function, "getPrototypeOf", Builtins::kObjectGetPrototypeOf,
        1, false);
    native_context()->set_object_get_prototype_of(*object_get_prototype_of);
    SimpleInstallFunction(object_function, "setPrototypeOf",
                          Builtins::kObjectSetPrototypeOf, 2, false);

    Handle<JSFunction> object_is_extensible = SimpleInstallFunction(
        object_function, "isExtensible", Builtins::kObjectIsExtensible,
        1, false);
    native_context()->set_object_is_extensible(*object_is_extensible);

    Handle<JSFunction> object_is_frozen = SimpleInstallFunction(
        object_function, "isFrozen", Builtins::kObjectIsFrozen, 1, false);
    native_context()->set_object_is_frozen(*object_is_frozen);

    Handle<JSFunction> object_is_sealed = SimpleInstallFunction(
        object_function, "isSealed", Builtins::kObjectIsSealed, 1, false);
    native_context()->set_object_is_sealed(*object_is_sealed);

    Handle<JSFunction> object_keys = SimpleInstallFunction(
        object_function, "keys", Builtins::kObjectKeys, 1, true);
    native_context()->set_object_keys(*object_keys);
    SimpleInstallFunction(object_function, factory->entries_string(),
                          Builtins::kObjectEntries, 1, false);
    SimpleInstallFunction(object_function, factory->values_string(),
                          Builtins::kObjectValues, 1, false);

    SimpleInstallFunction(isolate->initial_object_prototype(),
                          "__defineGetter__", Builtins::kObjectDefineGetter, 2,
                          true);
    SimpleInstallFunction(isolate->initial_object_prototype(),
                          "__defineSetter__", Builtins::kObjectDefineSetter, 2,
                          true);
    SimpleInstallFunction(isolate->initial_object_prototype(), "hasOwnProperty",
                          Builtins::kObjectHasOwnProperty, 1, true);
    SimpleInstallFunction(isolate->initial_object_prototype(),
                          "__lookupGetter__", Builtins::kObjectLookupGetter, 1,
                          true);
    SimpleInstallFunction(isolate->initial_object_prototype(),
                          "__lookupSetter__", Builtins::kObjectLookupSetter, 1,
                          true);
    SimpleInstallFunction(
        isolate->initial_object_prototype(), "propertyIsEnumerable",
        Builtins::kObjectPrototypePropertyIsEnumerable, 1, false);
    Handle<JSFunction> object_to_string = SimpleInstallFunction(
        isolate->initial_object_prototype(), factory->toString_string(),
        Builtins::kObjectProtoToString, 0, true);
    native_context()->set_object_to_string(*object_to_string);
    Handle<JSFunction> object_value_of = SimpleInstallFunction(
        isolate->initial_object_prototype(), "valueOf",
        Builtins::kObjectPrototypeValueOf, 0, true);
    native_context()->set_object_value_of(*object_value_of);

    SimpleInstallGetterSetter(isolate->initial_object_prototype(),
                              factory->proto_string(),
                              Builtins::kObjectPrototypeGetProto,
                              Builtins::kObjectPrototypeSetProto, DONT_ENUM);
  }

  Handle<JSObject> global(native_context()->global_object());

  {  // --- F u n c t i o n ---
    Handle<JSFunction> prototype = empty_function;
    Handle<JSFunction> function_fun =
        InstallFunction(global, "Function", JS_FUNCTION_TYPE, JSFunction::kSize,
                        prototype, Builtins::kFunctionConstructor);
    function_fun->set_prototype_or_initial_map(
        *sloppy_function_map_writable_prototype_);
    function_fun->shared()->DontAdaptArguments();
    function_fun->shared()->SetConstructStub(
        *isolate->builtins()->FunctionConstructor());
    function_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, function_fun,
                                     Context::FUNCTION_FUNCTION_INDEX);

    // Setup the methods on the %FunctionPrototype%.
    SimpleInstallFunction(prototype, factory->apply_string(),
                          Builtins::kFunctionPrototypeApply, 2, false);
    SimpleInstallFunction(prototype, factory->bind_string(),
                          Builtins::kFastFunctionPrototypeBind, 1, false);
    SimpleInstallFunction(prototype, factory->call_string(),
                          Builtins::kFunctionPrototypeCall, 1, false);
    SimpleInstallFunction(prototype, factory->toString_string(),
                          Builtins::kFunctionPrototypeToString, 0, false);

    // Install the @@hasInstance function.
    Handle<JSFunction> has_instance = InstallFunction(
        prototype, factory->has_instance_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, MaybeHandle<JSObject>(),
        Builtins::kFunctionPrototypeHasInstance,
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY));
    has_instance->shared()->set_builtin_function_id(kFunctionHasInstance);
    native_context()->set_function_has_instance(*has_instance);

    // Set the expected parameters for @@hasInstance to 1; required by builtin.
    has_instance->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    has_instance->shared()->set_length(1);

    // Install the "constructor" property on the %FunctionPrototype%.
    JSObject::AddProperty(prototype, factory->constructor_string(),
                          function_fun, DONT_ENUM);

    sloppy_function_map_writable_prototype_->SetConstructor(*function_fun);
    strict_function_map_writable_prototype_->SetConstructor(*function_fun);
    class_function_map_->SetConstructor(*function_fun);

    JSObject::MigrateSlowToFast(function_fun, 0, "Bootstrapping");
  }

  {
    // --- A s y n c F r o m S y n c I t e r a t o r
    Handle<Code> code = isolate->builtins()->AsyncIteratorValueUnwrap();
    Handle<SharedFunctionInfo> info =
        factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
    info->set_internal_formal_parameter_count(1);
    info->set_length(1);
    native_context()->set_async_iterator_value_unwrap_shared_fun(*info);
  }

  {  // --- A s y n c G e n e r a t o r ---
    Handle<JSFunction> await_caught =
        SimpleCreateFunction(isolate, factory->empty_string(),
                             Builtins::kAsyncGeneratorAwaitCaught, 2, false);
    InstallWithIntrinsicDefaultProto(isolate, await_caught,
                                     Context::ASYNC_GENERATOR_AWAIT_CAUGHT);

    Handle<JSFunction> await_uncaught =
        SimpleCreateFunction(isolate, factory->empty_string(),
                             Builtins::kAsyncGeneratorAwaitUncaught, 2, false);
    InstallWithIntrinsicDefaultProto(isolate, await_uncaught,
                                     Context::ASYNC_GENERATOR_AWAIT_UNCAUGHT);

    Handle<Code> code =
        isolate->builtins()->AsyncGeneratorAwaitResolveClosure();
    Handle<SharedFunctionInfo> info =
        factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
    info->set_internal_formal_parameter_count(1);
    info->set_length(1);
    native_context()->set_async_generator_await_resolve_shared_fun(*info);

    code = handle(isolate->builtins()->builtin(
                      Builtins::kAsyncGeneratorAwaitRejectClosure),
                  isolate);
    info = factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
    info->set_internal_formal_parameter_count(1);
    info->set_length(1);
    native_context()->set_async_generator_await_reject_shared_fun(*info);
  }

  {  // --- A r r a y ---
    Handle<JSFunction> array_function =
        InstallFunction(global, "Array", JS_ARRAY_TYPE, JSArray::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kArrayCode);
    array_function->shared()->DontAdaptArguments();
    array_function->shared()->set_builtin_function_id(kArrayCode);

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared()->set_length(1);

    Handle<Map> initial_map(array_function->initial_map());

    // This assert protects an optimization in
    // HGraphBuilder::JSArrayBuilder::EmitMapCode()
    DCHECK(initial_map->elements_kind() == GetInitialFastElementsKind());
    Map::EnsureDescriptorSlack(initial_map, 1);

    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE);

    Handle<AccessorInfo> array_length =
        Accessors::ArrayLengthInfo(isolate, attribs);
    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(array_length->name())), array_length,
          attribs);
      initial_map->AppendDescriptor(&d);
    }

    InstallWithIntrinsicDefaultProto(isolate, array_function,
                                     Context::ARRAY_FUNCTION_INDEX);
    InstallSpeciesGetter(array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(native_context(), initial_map);
    ArrayConstructorStub array_constructor_stub(isolate);
    Handle<Code> code = array_constructor_stub.GetCode();
    array_function->shared()->SetConstructStub(*code);

    // Set up %ArrayPrototype%.
    Handle<JSArray> array_prototype =
        Handle<JSArray>::cast(factory->NewJSObject(array_function, TENURED));
    JSArray::Initialize(array_prototype, 0);
    JSFunction::SetPrototype(array_function, array_prototype);
    native_context()->set_initial_array_prototype(*array_prototype);

    Handle<JSFunction> is_arraylike = SimpleInstallFunction(
        array_function, isolate->factory()->InternalizeUtf8String("isArray"),
        Builtins::kArrayIsArray, 1, true);
    native_context()->set_is_arraylike(*is_arraylike);
  }

  {  // --- A r r a y I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype());

    Handle<JSObject> array_iterator_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(array_iterator_prototype, iterator_prototype);

    JSObject::AddProperty(
        array_iterator_prototype, factory->to_string_tag_symbol(),
        factory->ArrayIterator_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> next = InstallFunction(
        array_iterator_prototype, "next", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        MaybeHandle<JSObject>(), Builtins::kArrayIteratorPrototypeNext);
    next->shared()->set_builtin_function_id(kArrayIteratorNext);

    // Set the expected parameters for %ArrayIteratorPrototype%.next to 0 (not
    // including the receiver), as required by the builtin.
    next->shared()->set_internal_formal_parameter_count(0);

    // Set the length for the function to satisfy ECMA-262.
    next->shared()->set_length(0);

    Handle<JSFunction> array_iterator_function = CreateFunction(
        isolate, factory->ArrayIterator_string(),
        JS_FAST_ARRAY_VALUE_ITERATOR_TYPE, JSArrayIterator::kSize,
        array_iterator_prototype, Builtins::kIllegal);
    array_iterator_function->shared()->set_native(false);
    array_iterator_function->shared()->set_instance_class_name(
        isolate->heap()->ArrayIterator_string());

    native_context()->set_initial_array_iterator_prototype(
        *array_iterator_prototype);
    native_context()->set_initial_array_iterator_prototype_map(
        array_iterator_prototype->map());

    Handle<Map> initial_map(array_iterator_function->initial_map(), isolate);

#define ARRAY_ITERATOR_LIST(V)                                              \
  V(TYPED_ARRAY, KEY, typed_array, key)                                     \
  V(FAST_ARRAY, KEY, fast_array, key)                                       \
  V(GENERIC_ARRAY, KEY, array, key)                                         \
  V(UINT8_ARRAY, KEY_VALUE, uint8_array, key_value)                         \
  V(INT8_ARRAY, KEY_VALUE, int8_array, key_value)                           \
  V(UINT16_ARRAY, KEY_VALUE, uint16_array, key_value)                       \
  V(INT16_ARRAY, KEY_VALUE, int16_array, key_value)                         \
  V(UINT32_ARRAY, KEY_VALUE, uint32_array, key_value)                       \
  V(INT32_ARRAY, KEY_VALUE, int32_array, key_value)                         \
  V(FLOAT32_ARRAY, KEY_VALUE, float32_array, key_value)                     \
  V(FLOAT64_ARRAY, KEY_VALUE, float64_array, key_value)                     \
  V(UINT8_CLAMPED_ARRAY, KEY_VALUE, uint8_clamped_array, key_value)         \
  V(FAST_SMI_ARRAY, KEY_VALUE, fast_smi_array, key_value)                   \
  V(FAST_HOLEY_SMI_ARRAY, KEY_VALUE, fast_holey_smi_array, key_value)       \
  V(FAST_ARRAY, KEY_VALUE, fast_array, key_value)                           \
  V(FAST_HOLEY_ARRAY, KEY_VALUE, fast_holey_array, key_value)               \
  V(FAST_DOUBLE_ARRAY, KEY_VALUE, fast_double_array, key_value)             \
  V(FAST_HOLEY_DOUBLE_ARRAY, KEY_VALUE, fast_holey_double_array, key_value) \
  V(GENERIC_ARRAY, KEY_VALUE, array, key_value)                             \
  V(UINT8_ARRAY, VALUE, uint8_array, value)                                 \
  V(INT8_ARRAY, VALUE, int8_array, value)                                   \
  V(UINT16_ARRAY, VALUE, uint16_array, value)                               \
  V(INT16_ARRAY, VALUE, int16_array, value)                                 \
  V(UINT32_ARRAY, VALUE, uint32_array, value)                               \
  V(INT32_ARRAY, VALUE, int32_array, value)                                 \
  V(FLOAT32_ARRAY, VALUE, float32_array, value)                             \
  V(FLOAT64_ARRAY, VALUE, float64_array, value)                             \
  V(UINT8_CLAMPED_ARRAY, VALUE, uint8_clamped_array, value)                 \
  V(FAST_SMI_ARRAY, VALUE, fast_smi_array, value)                           \
  V(FAST_HOLEY_SMI_ARRAY, VALUE, fast_holey_smi_array, value)               \
  V(FAST_ARRAY, VALUE, fast_array, value)                                   \
  V(FAST_HOLEY_ARRAY, VALUE, fast_holey_array, value)                       \
  V(FAST_DOUBLE_ARRAY, VALUE, fast_double_array, value)                     \
  V(FAST_HOLEY_DOUBLE_ARRAY, VALUE, fast_holey_double_array, value)         \
  V(GENERIC_ARRAY, VALUE, array, value)

#define CREATE_ARRAY_ITERATOR_MAP(PREFIX, SUFFIX, prefix, suffix)           \
  do {                                                                      \
    const InstanceType type = JS_##PREFIX##_##SUFFIX##_ITERATOR_TYPE;       \
    Handle<Map> map =                                                       \
        Map::Copy(initial_map, "JS_" #PREFIX "_" #SUFFIX "_ITERATOR_TYPE"); \
    map->set_instance_type(type);                                           \
    native_context()->set_##prefix##_##suffix##_iterator_map(*map);         \
  } while (0);

    ARRAY_ITERATOR_LIST(CREATE_ARRAY_ITERATOR_MAP)

#undef CREATE_ARRAY_ITERATOR_MAP
#undef ARRAY_ITERATOR_LIST
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun = InstallFunction(
        global, "Number", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kNumberConstructor);
    number_fun->shared()->DontAdaptArguments();
    number_fun->shared()->SetConstructStub(
        *isolate->builtins()->NumberConstructor_ConstructStub());
    number_fun->shared()->set_length(1);
    // https://tc39.github.io/ecma262/#sec-built-in-function-objects says
    // that "Built-in functions that are ECMAScript function objects must
    // be strict functions".
    number_fun->shared()->set_language_mode(STRICT);
    InstallWithIntrinsicDefaultProto(isolate, number_fun,
                                     Context::NUMBER_FUNCTION_INDEX);

    // Create the %NumberPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(number_fun, TENURED));
    prototype->set_value(Smi::kZero);
    JSFunction::SetPrototype(number_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), number_fun,
                          DONT_ENUM);

    // Install the Number.prototype methods.
    SimpleInstallFunction(prototype, "toExponential",
                          Builtins::kNumberPrototypeToExponential, 1, false);
    SimpleInstallFunction(prototype, "toFixed",
                          Builtins::kNumberPrototypeToFixed, 1, false);
    SimpleInstallFunction(prototype, "toPrecision",
                          Builtins::kNumberPrototypeToPrecision, 1, false);
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kNumberPrototypeToString, 1, false);
    SimpleInstallFunction(prototype, "valueOf",
                          Builtins::kNumberPrototypeValueOf, 0, true);

    // Install Intl fallback functions.
    SimpleInstallFunction(prototype, "toLocaleString",
                          Builtins::kNumberPrototypeToLocaleString, 0, false);

    // Install the Number functions.
    SimpleInstallFunction(number_fun, "isFinite", Builtins::kNumberIsFinite, 1,
                          true);
    SimpleInstallFunction(number_fun, "isInteger", Builtins::kNumberIsInteger,
                          1, true);
    SimpleInstallFunction(number_fun, "isNaN", Builtins::kNumberIsNaN, 1, true);
    SimpleInstallFunction(number_fun, "isSafeInteger",
                          Builtins::kNumberIsSafeInteger, 1, true);

    // Install Number.parseFloat and Global.parseFloat.
    Handle<JSFunction> parse_float_fun = SimpleInstallFunction(
        number_fun, "parseFloat", Builtins::kNumberParseFloat, 1, true);
    JSObject::AddProperty(global_object,
                          factory->NewStringFromAsciiChecked("parseFloat"),
                          parse_float_fun, DONT_ENUM);

    // Install Number.parseInt and Global.parseInt.
    Handle<JSFunction> parse_int_fun = SimpleInstallFunction(
        number_fun, "parseInt", Builtins::kNumberParseInt, 2, true);
    JSObject::AddProperty(global_object,
                          factory->NewStringFromAsciiChecked("parseInt"),
                          parse_int_fun, DONT_ENUM);

    // Install Number constants
    double kMaxValue = 1.7976931348623157e+308;
    double kMinValue = 5e-324;

    double kMaxSafeInt = 9007199254740991;
    double kMinSafeInt = -9007199254740991;
    double kEPS = 2.220446049250313e-16;

    Handle<Object> infinity = factory->infinity_value();
    Handle<Object> nan = factory->nan_value();
    Handle<String> nan_name = factory->NewStringFromAsciiChecked("NaN");

    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("MAX_VALUE"),
        factory->NewNumber(kMaxValue),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("MIN_VALUE"),
        factory->NewNumber(kMinValue),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, nan_name, nan,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("NEGATIVE_INFINITY"),
        factory->NewNumber(-V8_INFINITY),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("POSITIVE_INFINITY"),
        infinity,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("MAX_SAFE_INTEGER"),
        factory->NewNumber(kMaxSafeInt),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("MIN_SAFE_INTEGER"),
        factory->NewNumber(kMinSafeInt),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        number_fun, factory->NewStringFromAsciiChecked("EPSILON"),
        factory->NewNumber(kEPS),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));

    JSObject::AddProperty(
        global, factory->NewStringFromAsciiChecked("Infinity"), infinity,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        global, nan_name, nan,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        global, factory->NewStringFromAsciiChecked("undefined"),
        factory->undefined_value(),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kBooleanConstructor);
    boolean_fun->shared()->DontAdaptArguments();
    boolean_fun->shared()->SetConstructStub(
        *isolate->builtins()->BooleanConstructor_ConstructStub());
    boolean_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, boolean_fun,
                                     Context::BOOLEAN_FUNCTION_INDEX);

    // Create the %BooleanPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(boolean_fun, TENURED));
    prototype->set_value(isolate->heap()->false_value());
    JSFunction::SetPrototype(boolean_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), boolean_fun,
                          DONT_ENUM);

    // Install the Boolean.prototype methods.
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kBooleanPrototypeToString, 0, true);
    SimpleInstallFunction(prototype, "valueOf",
                          Builtins::kBooleanPrototypeValueOf, 0, true);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun = InstallFunction(
        global, "String", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kStringConstructor);
    string_fun->shared()->SetConstructStub(
        *isolate->builtins()->StringConstructor_ConstructStub());
    string_fun->shared()->DontAdaptArguments();
    string_fun->shared()->set_length(1);
    // https://tc39.github.io/ecma262/#sec-built-in-function-objects says
    // that "Built-in functions that are ECMAScript function objects must
    // be strict functions".
    string_fun->shared()->set_language_mode(STRICT);
    InstallWithIntrinsicDefaultProto(isolate, string_fun,
                                     Context::STRING_FUNCTION_INDEX);

    Handle<Map> string_map =
        Handle<Map>(native_context()->string_function()->initial_map());
    string_map->set_elements_kind(FAST_STRING_WRAPPER_ELEMENTS);
    Map::EnsureDescriptorSlack(string_map, 1);

    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<AccessorInfo> string_length(
        Accessors::StringLengthInfo(isolate, attribs));

    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(factory->length_string(),
                                                  string_length, attribs);
      string_map->AppendDescriptor(&d);
    }

    // Install the String.fromCharCode function.
    SimpleInstallFunction(string_fun, "fromCharCode",
                          Builtins::kStringFromCharCode, 1, false);

    // Install the String.fromCodePoint function.
    SimpleInstallFunction(string_fun, "fromCodePoint",
                          Builtins::kStringFromCodePoint, 1, false);

    // Create the %StringPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(string_fun, TENURED));
    prototype->set_value(isolate->heap()->empty_string());
    JSFunction::SetPrototype(string_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), string_fun,
                          DONT_ENUM);

    // Install the String.prototype methods.
    SimpleInstallFunction(prototype, "charAt", Builtins::kStringPrototypeCharAt,
                          1, true);
    SimpleInstallFunction(prototype, "charCodeAt",
                          Builtins::kStringPrototypeCharCodeAt, 1, true);
    SimpleInstallFunction(prototype, "concat", Builtins::kStringPrototypeConcat,
                          1, false);
    SimpleInstallFunction(prototype, "endsWith",
                          Builtins::kStringPrototypeEndsWith, 1, false);
    SimpleInstallFunction(prototype, "includes",
                          Builtins::kStringPrototypeIncludes, 1, false);
    SimpleInstallFunction(prototype, "indexOf",
                          Builtins::kStringPrototypeIndexOf, 1, false);
    SimpleInstallFunction(prototype, "lastIndexOf",
                          Builtins::kStringPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(prototype, "localeCompare",
                          Builtins::kStringPrototypeLocaleCompare, 1, true);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(prototype, "normalize",
                          Builtins::kStringPrototypeNormalizeIntl, 0, false);
#else
    SimpleInstallFunction(prototype, "normalize",
                          Builtins::kStringPrototypeNormalize, 0, false);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(prototype, "replace",
                          Builtins::kStringPrototypeReplace, 2, true);
    SimpleInstallFunction(prototype, "slice", Builtins::kStringPrototypeSlice,
                          2, false);
    SimpleInstallFunction(prototype, "split", Builtins::kStringPrototypeSplit,
                          2, true);
    SimpleInstallFunction(prototype, "substr", Builtins::kStringPrototypeSubstr,
                          2, true);
    SimpleInstallFunction(prototype, "substring",
                          Builtins::kStringPrototypeSubstring, 2, true);
    SimpleInstallFunction(prototype, "startsWith",
                          Builtins::kStringPrototypeStartsWith, 1, false);
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kStringPrototypeToString, 0, true);
    SimpleInstallFunction(prototype, "trim", Builtins::kStringPrototypeTrim, 0,
                          false);
    SimpleInstallFunction(prototype, "trimLeft",
                          Builtins::kStringPrototypeTrimLeft, 0, false);
    SimpleInstallFunction(prototype, "trimRight",
                          Builtins::kStringPrototypeTrimRight, 0, false);
    SimpleInstallFunction(prototype, "toLocaleLowerCase",
                          Builtins::kStringPrototypeToLocaleLowerCase, 0,
                          false);
    SimpleInstallFunction(prototype, "toLocaleUpperCase",
                          Builtins::kStringPrototypeToLocaleUpperCase, 0,
                          false);
    SimpleInstallFunction(prototype, "toLowerCase",
                          Builtins::kStringPrototypeToLowerCase, 0, false);
    SimpleInstallFunction(prototype, "toUpperCase",
                          Builtins::kStringPrototypeToUpperCase, 0, false);
    SimpleInstallFunction(prototype, "valueOf",
                          Builtins::kStringPrototypeValueOf, 0, true);

    Handle<JSFunction> iterator = SimpleCreateFunction(
        isolate, factory->NewStringFromAsciiChecked("[Symbol.iterator]"),
        Builtins::kStringPrototypeIterator, 0, true);
    iterator->shared()->set_builtin_function_id(kStringIterator);
    JSObject::AddProperty(prototype, factory->iterator_symbol(), iterator,
                          static_cast<PropertyAttributes>(DONT_ENUM));
  }

  {  // --- S t r i n g I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype());

    Handle<JSObject> string_iterator_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(string_iterator_prototype, iterator_prototype);

    JSObject::AddProperty(
        string_iterator_prototype, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("String Iterator"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> next =
        InstallFunction(string_iterator_prototype, "next", JS_OBJECT_TYPE,
                        JSObject::kHeaderSize, MaybeHandle<JSObject>(),
                        Builtins::kStringIteratorPrototypeNext);
    next->shared()->set_builtin_function_id(kStringIteratorNext);

    // Set the expected parameters for %StringIteratorPrototype%.next to 0 (not
    // including the receiver), as required by the builtin.
    next->shared()->set_internal_formal_parameter_count(0);

    // Set the length for the function to satisfy ECMA-262.
    next->shared()->set_length(0);

    Handle<JSFunction> string_iterator_function = CreateFunction(
        isolate, factory->NewStringFromAsciiChecked("StringIterator"),
        JS_STRING_ITERATOR_TYPE, JSStringIterator::kSize,
        string_iterator_prototype, Builtins::kIllegal);
    string_iterator_function->shared()->set_native(false);
    native_context()->set_string_iterator_map(
        string_iterator_function->initial_map());
  }

  {
    // --- S y m b o l ---
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> symbol_fun =
        InstallFunction(global, "Symbol", JS_VALUE_TYPE, JSValue::kSize,
                        prototype, Builtins::kSymbolConstructor);
    symbol_fun->shared()->SetConstructStub(
        *isolate->builtins()->SymbolConstructor_ConstructStub());
    symbol_fun->shared()->set_length(0);
    symbol_fun->shared()->DontAdaptArguments();
    native_context()->set_symbol_function(*symbol_fun);

    // Install the Symbol.for and Symbol.keyFor functions.
    SimpleInstallFunction(symbol_fun, "for", Builtins::kSymbolFor, 1, false);
    SimpleInstallFunction(symbol_fun, "keyFor", Builtins::kSymbolKeyFor, 1,
                          false);

    // Install well-known symbols.
    InstallConstant(isolate, symbol_fun, "hasInstance",
                    factory->has_instance_symbol());
    InstallConstant(isolate, symbol_fun, "isConcatSpreadable",
                    factory->is_concat_spreadable_symbol());
    InstallConstant(isolate, symbol_fun, "iterator",
                    factory->iterator_symbol());
    InstallConstant(isolate, symbol_fun, "match", factory->match_symbol());
    InstallConstant(isolate, symbol_fun, "replace", factory->replace_symbol());
    InstallConstant(isolate, symbol_fun, "search", factory->search_symbol());
    InstallConstant(isolate, symbol_fun, "species", factory->species_symbol());
    InstallConstant(isolate, symbol_fun, "split", factory->split_symbol());
    InstallConstant(isolate, symbol_fun, "toPrimitive",
                    factory->to_primitive_symbol());
    InstallConstant(isolate, symbol_fun, "toStringTag",
                    factory->to_string_tag_symbol());
    InstallConstant(isolate, symbol_fun, "unscopables",
                    factory->unscopables_symbol());

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        prototype, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("Symbol"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), symbol_fun,
                          DONT_ENUM);

    // Install the Symbol.prototype methods.
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kSymbolPrototypeToString, 0, true);
    SimpleInstallFunction(prototype, "valueOf",
                          Builtins::kSymbolPrototypeValueOf, 0, true);

    // Install the @@toPrimitive function.
    Handle<JSFunction> to_primitive = InstallFunction(
        prototype, factory->to_primitive_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, MaybeHandle<JSObject>(),
        Builtins::kSymbolPrototypeToPrimitive,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Set the expected parameters for @@toPrimitive to 1; required by builtin.
    to_primitive->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    to_primitive->shared()->set_length(1);
  }

  {  // --- D a t e ---
    // Builtin functions for Date.prototype.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> date_fun =
        InstallFunction(global, "Date", JS_DATE_TYPE, JSDate::kSize, prototype,
                        Builtins::kDateConstructor);
    InstallWithIntrinsicDefaultProto(isolate, date_fun,
                                     Context::DATE_FUNCTION_INDEX);
    date_fun->shared()->SetConstructStub(
        *isolate->builtins()->DateConstructor_ConstructStub());
    date_fun->shared()->set_length(7);
    date_fun->shared()->DontAdaptArguments();

    // Install the Date.now, Date.parse and Date.UTC functions.
    SimpleInstallFunction(date_fun, "now", Builtins::kDateNow, 0, false);
    SimpleInstallFunction(date_fun, "parse", Builtins::kDateParse, 1, false);
    SimpleInstallFunction(date_fun, "UTC", Builtins::kDateUTC, 7, false);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), date_fun,
                          DONT_ENUM);

    // Install the Date.prototype methods.
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(prototype, "toDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(prototype, "toTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);
    SimpleInstallFunction(prototype, "toISOString",
                          Builtins::kDatePrototypeToISOString, 0, false);
    Handle<JSFunction> to_utc_string =
        SimpleInstallFunction(prototype, "toUTCString",
                              Builtins::kDatePrototypeToUTCString, 0, false);
    InstallFunction(prototype, to_utc_string,
                    factory->InternalizeUtf8String("toGMTString"), DONT_ENUM);
    SimpleInstallFunction(prototype, "getDate", Builtins::kDatePrototypeGetDate,
                          0, true);
    SimpleInstallFunction(prototype, "setDate", Builtins::kDatePrototypeSetDate,
                          1, false);
    SimpleInstallFunction(prototype, "getDay", Builtins::kDatePrototypeGetDay,
                          0, true);
    SimpleInstallFunction(prototype, "getFullYear",
                          Builtins::kDatePrototypeGetFullYear, 0, true);
    SimpleInstallFunction(prototype, "setFullYear",
                          Builtins::kDatePrototypeSetFullYear, 3, false);
    SimpleInstallFunction(prototype, "getHours",
                          Builtins::kDatePrototypeGetHours, 0, true);
    SimpleInstallFunction(prototype, "setHours",
                          Builtins::kDatePrototypeSetHours, 4, false);
    SimpleInstallFunction(prototype, "getMilliseconds",
                          Builtins::kDatePrototypeGetMilliseconds, 0, true);
    SimpleInstallFunction(prototype, "setMilliseconds",
                          Builtins::kDatePrototypeSetMilliseconds, 1, false);
    SimpleInstallFunction(prototype, "getMinutes",
                          Builtins::kDatePrototypeGetMinutes, 0, true);
    SimpleInstallFunction(prototype, "setMinutes",
                          Builtins::kDatePrototypeSetMinutes, 3, false);
    SimpleInstallFunction(prototype, "getMonth",
                          Builtins::kDatePrototypeGetMonth, 0, true);
    SimpleInstallFunction(prototype, "setMonth",
                          Builtins::kDatePrototypeSetMonth, 2, false);
    SimpleInstallFunction(prototype, "getSeconds",
                          Builtins::kDatePrototypeGetSeconds, 0, true);
    SimpleInstallFunction(prototype, "setSeconds",
                          Builtins::kDatePrototypeSetSeconds, 2, false);
    SimpleInstallFunction(prototype, "getTime", Builtins::kDatePrototypeGetTime,
                          0, true);
    SimpleInstallFunction(prototype, "setTime", Builtins::kDatePrototypeSetTime,
                          1, false);
    SimpleInstallFunction(prototype, "getTimezoneOffset",
                          Builtins::kDatePrototypeGetTimezoneOffset, 0, true);
    SimpleInstallFunction(prototype, "getUTCDate",
                          Builtins::kDatePrototypeGetUTCDate, 0, true);
    SimpleInstallFunction(prototype, "setUTCDate",
                          Builtins::kDatePrototypeSetUTCDate, 1, false);
    SimpleInstallFunction(prototype, "getUTCDay",
                          Builtins::kDatePrototypeGetUTCDay, 0, true);
    SimpleInstallFunction(prototype, "getUTCFullYear",
                          Builtins::kDatePrototypeGetUTCFullYear, 0, true);
    SimpleInstallFunction(prototype, "setUTCFullYear",
                          Builtins::kDatePrototypeSetUTCFullYear, 3, false);
    SimpleInstallFunction(prototype, "getUTCHours",
                          Builtins::kDatePrototypeGetUTCHours, 0, true);
    SimpleInstallFunction(prototype, "setUTCHours",
                          Builtins::kDatePrototypeSetUTCHours, 4, false);
    SimpleInstallFunction(prototype, "getUTCMilliseconds",
                          Builtins::kDatePrototypeGetUTCMilliseconds, 0, true);
    SimpleInstallFunction(prototype, "setUTCMilliseconds",
                          Builtins::kDatePrototypeSetUTCMilliseconds, 1, false);
    SimpleInstallFunction(prototype, "getUTCMinutes",
                          Builtins::kDatePrototypeGetUTCMinutes, 0, true);
    SimpleInstallFunction(prototype, "setUTCMinutes",
                          Builtins::kDatePrototypeSetUTCMinutes, 3, false);
    SimpleInstallFunction(prototype, "getUTCMonth",
                          Builtins::kDatePrototypeGetUTCMonth, 0, true);
    SimpleInstallFunction(prototype, "setUTCMonth",
                          Builtins::kDatePrototypeSetUTCMonth, 2, false);
    SimpleInstallFunction(prototype, "getUTCSeconds",
                          Builtins::kDatePrototypeGetUTCSeconds, 0, true);
    SimpleInstallFunction(prototype, "setUTCSeconds",
                          Builtins::kDatePrototypeSetUTCSeconds, 2, false);
    SimpleInstallFunction(prototype, "valueOf", Builtins::kDatePrototypeValueOf,
                          0, true);
    SimpleInstallFunction(prototype, "getYear", Builtins::kDatePrototypeGetYear,
                          0, true);
    SimpleInstallFunction(prototype, "setYear", Builtins::kDatePrototypeSetYear,
                          1, false);
    SimpleInstallFunction(prototype, "toJSON", Builtins::kDatePrototypeToJson,
                          1, false);

    // Install Intl fallback functions.
    SimpleInstallFunction(prototype, "toLocaleString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(prototype, "toLocaleDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(prototype, "toLocaleTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);

    // Install the @@toPrimitive function.
    Handle<JSFunction> to_primitive = InstallFunction(
        prototype, factory->to_primitive_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, MaybeHandle<JSObject>(),
        Builtins::kDatePrototypeToPrimitive,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Set the expected parameters for @@toPrimitive to 1; required by builtin.
    to_primitive->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    to_primitive->shared()->set_length(1);
  }

  {
    Handle<Code> code = isolate->builtins()->PromiseGetCapabilitiesExecutor();
    Handle<SharedFunctionInfo> info =
        factory->NewSharedFunctionInfo(factory->empty_string(), code, true);
    info->SetConstructStub(*isolate->builtins()->JSBuiltinsConstructStub());
    info->set_instance_class_name(isolate->heap()->Object_string());
    info->set_internal_formal_parameter_count(2);
    info->set_length(2);
    native_context()->set_promise_get_capabilities_executor_shared_fun(*info);

    // %new_promise_capability(C, debugEvent)
    Handle<JSFunction> new_promise_capability =
        SimpleCreateFunction(isolate, factory->empty_string(),
                             Builtins::kNewPromiseCapability, 2, false);
    InstallWithIntrinsicDefaultProto(isolate, new_promise_capability,
                                     Context::NEW_PROMISE_CAPABILITY_INDEX);
  }

  {  // -- P r o m i s e
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> promise_fun = InstallFunction(
        global, "Promise", JS_PROMISE_TYPE, JSPromise::kSizeWithEmbedderFields,
        prototype, Builtins::kPromiseConstructor);
    InstallWithIntrinsicDefaultProto(isolate, promise_fun,
                                     Context::PROMISE_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(promise_fun->shared(), isolate);
    shared->SetConstructStub(*isolate->builtins()->JSBuiltinsConstructStub());
    shared->set_instance_class_name(isolate->heap()->Object_string());
    shared->set_internal_formal_parameter_count(1);
    shared->set_length(1);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), promise_fun,
                          DONT_ENUM);

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        prototype, factory->to_string_tag_symbol(), factory->Promise_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> promise_then =
        SimpleInstallFunction(prototype, isolate->factory()->then_string(),
                              Builtins::kPromiseThen, 2, true);
    InstallWithIntrinsicDefaultProto(isolate, promise_then,
                                     Context::PROMISE_THEN_INDEX);

    Handle<JSFunction> promise_catch = SimpleInstallFunction(
        prototype, "catch", Builtins::kPromiseCatch, 1, true, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(isolate, promise_catch,
                                     Context::PROMISE_CATCH_INDEX);

    InstallSpeciesGetter(promise_fun);

    SimpleInstallFunction(promise_fun, "resolve", Builtins::kPromiseResolve, 1,
                          true, DONT_ENUM);

    SimpleInstallFunction(promise_fun, "reject", Builtins::kPromiseReject, 1,
                          true, DONT_ENUM);

    Handle<Map> prototype_map(prototype->map());
    Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate);

    // Store the initial Promise.prototype map. This is used in fast-path
    // checks. Do not alter the prototype after this point.
    native_context()->set_promise_prototype_map(*prototype_map);

    {  // Internal: PromiseInternalConstructor
       // Also exposed as extrasUtils.createPromise.
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kPromiseInternalConstructor, 1, true);
      function->shared()->set_native(false);
      InstallWithIntrinsicDefaultProto(
          isolate, function, Context::PROMISE_INTERNAL_CONSTRUCTOR_INDEX);
    }

    {  // Internal: IsPromise
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate, factory->empty_string(), Builtins::kIsPromise, 1, false);
      InstallWithIntrinsicDefaultProto(isolate, function,
                                       Context::IS_PROMISE_INDEX);
    }

    {  // Internal: ResolvePromise
       // Also exposed as extrasUtils.resolvePromise.
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate, factory->empty_string(), Builtins::kResolvePromise, 2, true);
      function->shared()->set_native(false);
      InstallWithIntrinsicDefaultProto(isolate, function,
                                       Context::PROMISE_RESOLVE_INDEX);
    }

    {  // Internal: PromiseHandle
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate, factory->empty_string(), Builtins::kPromiseHandle, 5, false);
      InstallWithIntrinsicDefaultProto(isolate, function,
                                       Context::PROMISE_HANDLE_INDEX);
    }

    {  // Internal: PromiseHandleReject
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kPromiseHandleReject, 3, false);
      InstallWithIntrinsicDefaultProto(isolate, function,
                                       Context::PROMISE_HANDLE_REJECT_INDEX);
    }

    {  // Internal: InternalPromiseReject
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kInternalPromiseReject, 3, true);
      function->shared()->set_native(false);
      InstallWithIntrinsicDefaultProto(isolate, function,
                                       Context::PROMISE_INTERNAL_REJECT_INDEX);
    }

    {
      Handle<Code> code =
          handle(isolate->builtins()->builtin(Builtins::kPromiseResolveClosure),
                 isolate);
      Handle<SharedFunctionInfo> info =
          factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
      info->set_internal_formal_parameter_count(1);
      info->set_length(1);
      native_context()->set_promise_resolve_shared_fun(*info);

      code =
          handle(isolate->builtins()->builtin(Builtins::kPromiseRejectClosure),
                 isolate);
      info =
          factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
      info->set_internal_formal_parameter_count(1);
      info->set_length(1);
      native_context()->set_promise_reject_shared_fun(*info);
    }

    JSObject::MigrateSlowToFast(promise_fun, 0, "Bootstrapping");
  }

  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        prototype, Builtins::kRegExpConstructor);
    InstallWithIntrinsicDefaultProto(isolate, regexp_fun,
                                     Context::REGEXP_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(regexp_fun->shared(), isolate);
    shared->SetConstructStub(*isolate->builtins()->JSBuiltinsConstructStub());
    shared->set_instance_class_name(isolate->heap()->RegExp_string());
    shared->set_internal_formal_parameter_count(2);
    shared->set_length(2);

    {
      // RegExp.prototype setup.

      // Install the "constructor" property on the {prototype}.
      JSObject::AddProperty(prototype, factory->constructor_string(),
                            regexp_fun, DONT_ENUM);

      {
        Handle<JSFunction> fun = SimpleInstallFunction(
            prototype, factory->exec_string(), Builtins::kRegExpPrototypeExec,
            1, true, DONT_ENUM);
        native_context()->set_regexp_exec_function(*fun);
      }

      SimpleInstallGetter(prototype, factory->flags_string(),
                          Builtins::kRegExpPrototypeFlagsGetter, true);
      SimpleInstallGetter(prototype, factory->global_string(),
                          Builtins::kRegExpPrototypeGlobalGetter, true);
      SimpleInstallGetter(prototype, factory->ignoreCase_string(),
                          Builtins::kRegExpPrototypeIgnoreCaseGetter, true);
      SimpleInstallGetter(prototype, factory->multiline_string(),
                          Builtins::kRegExpPrototypeMultilineGetter, true);
      SimpleInstallGetter(prototype, factory->source_string(),
                          Builtins::kRegExpPrototypeSourceGetter, true);
      SimpleInstallGetter(prototype, factory->sticky_string(),
                          Builtins::kRegExpPrototypeStickyGetter, true);
      SimpleInstallGetter(prototype, factory->unicode_string(),
                          Builtins::kRegExpPrototypeUnicodeGetter, true);

      SimpleInstallFunction(prototype, "compile",
                            Builtins::kRegExpPrototypeCompile, 2, true,
                            DONT_ENUM);
      SimpleInstallFunction(prototype, factory->toString_string(),
                            Builtins::kRegExpPrototypeToString, 0, false,
                            DONT_ENUM);
      SimpleInstallFunction(prototype, "test", Builtins::kRegExpPrototypeTest,
                            1, true, DONT_ENUM);

      {
        Handle<JSFunction> fun = SimpleCreateFunction(
            isolate, factory->InternalizeUtf8String("[Symbol.match]"),
            Builtins::kRegExpPrototypeMatch, 1, true);
        InstallFunction(prototype, fun, factory->match_symbol(), DONT_ENUM);
      }

      {
        Handle<JSFunction> fun = SimpleCreateFunction(
            isolate, factory->InternalizeUtf8String("[Symbol.replace]"),
            Builtins::kRegExpPrototypeReplace, 2, false);
        InstallFunction(prototype, fun, factory->replace_symbol(), DONT_ENUM);
      }

      {
        Handle<JSFunction> fun = SimpleCreateFunction(
            isolate, factory->InternalizeUtf8String("[Symbol.search]"),
            Builtins::kRegExpPrototypeSearch, 1, true);
        InstallFunction(prototype, fun, factory->search_symbol(), DONT_ENUM);
      }

      {
        Handle<JSFunction> fun = SimpleCreateFunction(
            isolate, factory->InternalizeUtf8String("[Symbol.split]"),
            Builtins::kRegExpPrototypeSplit, 2, false);
        InstallFunction(prototype, fun, factory->split_symbol(), DONT_ENUM);
      }

      Handle<Map> prototype_map(prototype->map());
      Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate);

      // Store the initial RegExp.prototype map. This is used in fast-path
      // checks. Do not alter the prototype after this point.
      native_context()->set_regexp_prototype_map(*prototype_map);
    }

    {
      // RegExp getters and setters.

      InstallSpeciesGetter(regexp_fun);

      // Static properties set by a successful match.

      const PropertyAttributes no_enum = DONT_ENUM;
      SimpleInstallGetterSetter(regexp_fun, factory->input_string(),
                                Builtins::kRegExpInputGetter,
                                Builtins::kRegExpInputSetter, no_enum);
      SimpleInstallGetterSetter(
          regexp_fun, factory->InternalizeUtf8String("$_"),
          Builtins::kRegExpInputGetter, Builtins::kRegExpInputSetter, no_enum);

      SimpleInstallGetterSetter(
          regexp_fun, factory->InternalizeUtf8String("lastMatch"),
          Builtins::kRegExpLastMatchGetter, Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(
          regexp_fun, factory->InternalizeUtf8String("$&"),
          Builtins::kRegExpLastMatchGetter, Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(
          regexp_fun, factory->InternalizeUtf8String("lastParen"),
          Builtins::kRegExpLastParenGetter, Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(
          regexp_fun, factory->InternalizeUtf8String("$+"),
          Builtins::kRegExpLastParenGetter, Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(regexp_fun,
                                factory->InternalizeUtf8String("leftContext"),
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(regexp_fun,
                                factory->InternalizeUtf8String("$`"),
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(regexp_fun,
                                factory->InternalizeUtf8String("rightContext"),
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(regexp_fun,
                                factory->InternalizeUtf8String("$'"),
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction, no_enum);

#define INSTALL_CAPTURE_GETTER(i)                         \
  SimpleInstallGetterSetter(                              \
      regexp_fun, factory->InternalizeUtf8String("$" #i), \
      Builtins::kRegExpCapture##i##Getter, Builtins::kEmptyFunction, no_enum)
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
    Handle<Map> initial_map(regexp_fun->initial_map());

    DCHECK_EQ(0, initial_map->GetInObjectProperties());

    Map::EnsureDescriptorSlack(initial_map, 1);

    // ECMA-262, section 15.10.7.5.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    Descriptor d = Descriptor::DataField(factory->lastIndex_string(),
                                         JSRegExp::kLastIndexFieldIndex,
                                         writable, Representation::Tagged());
    initial_map->AppendDescriptor(&d);

    static const int num_fields = JSRegExp::kInObjectFieldCount;
    initial_map->SetInObjectProperties(num_fields);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(initial_map->instance_size() +
                                   num_fields * kPointerSize);

    {  // Internal: RegExpInternalMatch
      Handle<JSFunction> function =
          factory->NewFunction(isolate->factory()->empty_string(),
                               isolate->builtins()->RegExpInternalMatch(),
                               JS_OBJECT_TYPE, JSObject::kHeaderSize);
      function->shared()->set_internal_formal_parameter_count(2);
      function->shared()->set_length(2);
      function->shared()->set_native(true);
      native_context()->set(Context::REGEXP_INTERNAL_MATCH, *function);
    }

    // Create the last match info. One for external use, and one for internal
    // use when we don't want to modify the externally visible match info.
    Handle<RegExpMatchInfo> last_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_last_match_info(*last_match_info);
    Handle<RegExpMatchInfo> internal_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_internal_match_info(*internal_match_info);

    // Force the RegExp constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof RegExp
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(regexp_fun, 0, "Bootstrapping");
  }

  {  // -- E r r o r
    InstallError(isolate, global, factory->Error_string(),
                 Context::ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate, isolate->builtins()->MakeError(),
                     Context::MAKE_ERROR_INDEX);
  }

  {  // -- E v a l E r r o r
    InstallError(isolate, global, factory->EvalError_string(),
                 Context::EVAL_ERROR_FUNCTION_INDEX);
  }

  {  // -- R a n g e E r r o r
    InstallError(isolate, global, factory->RangeError_string(),
                 Context::RANGE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate, isolate->builtins()->MakeRangeError(),
                     Context::MAKE_RANGE_ERROR_INDEX);
  }

  {  // -- R e f e r e n c e E r r o r
    InstallError(isolate, global, factory->ReferenceError_string(),
                 Context::REFERENCE_ERROR_FUNCTION_INDEX);
  }

  {  // -- S y n t a x E r r o r
    InstallError(isolate, global, factory->SyntaxError_string(),
                 Context::SYNTAX_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate, isolate->builtins()->MakeSyntaxError(),
                     Context::MAKE_SYNTAX_ERROR_INDEX);
  }

  {  // -- T y p e E r r o r
    InstallError(isolate, global, factory->TypeError_string(),
                 Context::TYPE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate, isolate->builtins()->MakeTypeError(),
                     Context::MAKE_TYPE_ERROR_INDEX);
  }

  {  // -- U R I E r r o r
    InstallError(isolate, global, factory->URIError_string(),
                 Context::URI_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate, isolate->builtins()->MakeURIError(),
                     Context::MAKE_URI_ERROR_INDEX);
  }

  {  // -- C o m p i l e E r r o r
    Handle<JSObject> dummy = factory->NewJSObject(isolate->object_function());
    InstallError(isolate, dummy, factory->CompileError_string(),
                 Context::WASM_COMPILE_ERROR_FUNCTION_INDEX);

    // -- L i n k E r r o r
    InstallError(isolate, dummy, factory->LinkError_string(),
                 Context::WASM_LINK_ERROR_FUNCTION_INDEX);

    // -- R u n t i m e E r r o r
    InstallError(isolate, dummy, factory->RuntimeError_string(),
                 Context::WASM_RUNTIME_ERROR_FUNCTION_INDEX);
  }

  // Initialize the embedder data slot.
  Handle<FixedArray> embedder_data = factory->NewFixedArray(3);
  native_context()->set_embedder_data(*embedder_data);

  {  // -- J S O N
    Handle<String> name = factory->InternalizeUtf8String("JSON");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
    Handle<JSObject> json_object = factory->NewJSObject(cons, TENURED);
    DCHECK(json_object->IsJSObject());
    JSObject::AddProperty(global, name, json_object, DONT_ENUM);
    SimpleInstallFunction(json_object, "parse", Builtins::kJsonParse, 2, false);
    SimpleInstallFunction(json_object, "stringify", Builtins::kJsonStringify, 3,
                          true);
    JSObject::AddProperty(
        json_object, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("JSON"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- M a t h
    Handle<String> name = factory->InternalizeUtf8String("Math");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
    Handle<JSObject> math = factory->NewJSObject(cons, TENURED);
    DCHECK(math->IsJSObject());
    JSObject::AddProperty(global, name, math, DONT_ENUM);
    SimpleInstallFunction(math, "abs", Builtins::kMathAbs, 1, true);
    SimpleInstallFunction(math, "acos", Builtins::kMathAcos, 1, true);
    SimpleInstallFunction(math, "acosh", Builtins::kMathAcosh, 1, true);
    SimpleInstallFunction(math, "asin", Builtins::kMathAsin, 1, true);
    SimpleInstallFunction(math, "asinh", Builtins::kMathAsinh, 1, true);
    SimpleInstallFunction(math, "atan", Builtins::kMathAtan, 1, true);
    SimpleInstallFunction(math, "atanh", Builtins::kMathAtanh, 1, true);
    SimpleInstallFunction(math, "atan2", Builtins::kMathAtan2, 2, true);
    SimpleInstallFunction(math, "ceil", Builtins::kMathCeil, 1, true);
    SimpleInstallFunction(math, "cbrt", Builtins::kMathCbrt, 1, true);
    SimpleInstallFunction(math, "expm1", Builtins::kMathExpm1, 1, true);
    SimpleInstallFunction(math, "clz32", Builtins::kMathClz32, 1, true);
    SimpleInstallFunction(math, "cos", Builtins::kMathCos, 1, true);
    SimpleInstallFunction(math, "cosh", Builtins::kMathCosh, 1, true);
    SimpleInstallFunction(math, "exp", Builtins::kMathExp, 1, true);
    Handle<JSFunction> math_floor =
        SimpleInstallFunction(math, "floor", Builtins::kMathFloor, 1, true);
    native_context()->set_math_floor(*math_floor);
    SimpleInstallFunction(math, "fround", Builtins::kMathFround, 1, true);
    SimpleInstallFunction(math, "hypot", Builtins::kMathHypot, 2, false);
    SimpleInstallFunction(math, "imul", Builtins::kMathImul, 2, true);
    SimpleInstallFunction(math, "log", Builtins::kMathLog, 1, true);
    SimpleInstallFunction(math, "log1p", Builtins::kMathLog1p, 1, true);
    SimpleInstallFunction(math, "log2", Builtins::kMathLog2, 1, true);
    SimpleInstallFunction(math, "log10", Builtins::kMathLog10, 1, true);
    SimpleInstallFunction(math, "max", Builtins::kMathMax, 2, false);
    SimpleInstallFunction(math, "min", Builtins::kMathMin, 2, false);
    Handle<JSFunction> math_pow =
        SimpleInstallFunction(math, "pow", Builtins::kMathPow, 2, true);
    native_context()->set_math_pow(*math_pow);
    SimpleInstallFunction(math, "random", Builtins::kMathRandom, 0, true);
    SimpleInstallFunction(math, "round", Builtins::kMathRound, 1, true);
    SimpleInstallFunction(math, "sign", Builtins::kMathSign, 1, true);
    SimpleInstallFunction(math, "sin", Builtins::kMathSin, 1, true);
    SimpleInstallFunction(math, "sinh", Builtins::kMathSinh, 1, true);
    SimpleInstallFunction(math, "sqrt", Builtins::kMathSqrt, 1, true);
    SimpleInstallFunction(math, "tan", Builtins::kMathTan, 1, true);
    SimpleInstallFunction(math, "tanh", Builtins::kMathTanh, 1, true);
    SimpleInstallFunction(math, "trunc", Builtins::kMathTrunc, 1, true);

    // Install math constants.
    double const kE = base::ieee754::exp(1.0);
    double const kPI = 3.1415926535897932;
    InstallConstant(isolate, math, "E", factory->NewNumber(kE));
    InstallConstant(isolate, math, "LN10",
                    factory->NewNumber(base::ieee754::log(10.0)));
    InstallConstant(isolate, math, "LN2",
                    factory->NewNumber(base::ieee754::log(2.0)));
    InstallConstant(isolate, math, "LOG10E",
                    factory->NewNumber(base::ieee754::log10(kE)));
    InstallConstant(isolate, math, "LOG2E",
                    factory->NewNumber(base::ieee754::log2(kE)));
    InstallConstant(isolate, math, "PI", factory->NewNumber(kPI));
    InstallConstant(isolate, math, "SQRT1_2",
                    factory->NewNumber(std::sqrt(0.5)));
    InstallConstant(isolate, math, "SQRT2", factory->NewNumber(std::sqrt(2.0)));
    JSObject::AddProperty(
        math, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("Math"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- C o n s o l e
    Handle<String> name = factory->InternalizeUtf8String("console");
    Handle<JSFunction> cons = factory->NewFunction(name);
    Handle<JSObject> empty = factory->NewJSObject(isolate->object_function());
    JSFunction::SetPrototype(cons, empty);
    Handle<JSObject> console = factory->NewJSObject(cons, TENURED);
    DCHECK(console->IsJSObject());
    JSObject::AddProperty(global, name, console, DONT_ENUM);
    SimpleInstallFunction(console, "debug", Builtins::kConsoleDebug, 1, false,
                          NONE);
    SimpleInstallFunction(console, "error", Builtins::kConsoleError, 1, false,
                          NONE);
    SimpleInstallFunction(console, "info", Builtins::kConsoleInfo, 1, false,
                          NONE);
    SimpleInstallFunction(console, "log", Builtins::kConsoleLog, 1, false,
                          NONE);
    SimpleInstallFunction(console, "warn", Builtins::kConsoleWarn, 1, false,
                          NONE);
    SimpleInstallFunction(console, "dir", Builtins::kConsoleDir, 1, false,
                          NONE);
    SimpleInstallFunction(console, "dirxml", Builtins::kConsoleDirXml, 1, false,
                          NONE);
    SimpleInstallFunction(console, "table", Builtins::kConsoleTable, 1, false,
                          NONE);
    SimpleInstallFunction(console, "trace", Builtins::kConsoleTrace, 1, false,
                          NONE);
    SimpleInstallFunction(console, "group", Builtins::kConsoleGroup, 1, false,
                          NONE);
    SimpleInstallFunction(console, "groupCollapsed",
                          Builtins::kConsoleGroupCollapsed, 1, false, NONE);
    SimpleInstallFunction(console, "groupEnd", Builtins::kConsoleGroupEnd, 1,
                          false, NONE);
    SimpleInstallFunction(console, "clear", Builtins::kConsoleClear, 1, false,
                          NONE);
    SimpleInstallFunction(console, "count", Builtins::kConsoleCount, 1, false,
                          NONE);
    SimpleInstallFunction(console, "assert", Builtins::kFastConsoleAssert, 1,
                          false, NONE);
    SimpleInstallFunction(console, "markTimeline",
                          Builtins::kConsoleMarkTimeline, 1, false, NONE);
    SimpleInstallFunction(console, "profile", Builtins::kConsoleProfile, 1,
                          false, NONE);
    SimpleInstallFunction(console, "profileEnd", Builtins::kConsoleProfileEnd,
                          1, false, NONE);
    SimpleInstallFunction(console, "timeline", Builtins::kConsoleTimeline, 1,
                          false, NONE);
    SimpleInstallFunction(console, "timelineEnd", Builtins::kConsoleTimelineEnd,
                          1, false, NONE);
    SimpleInstallFunction(console, "time", Builtins::kConsoleTime, 1, false,
                          NONE);
    SimpleInstallFunction(console, "timeEnd", Builtins::kConsoleTimeEnd, 1,
                          false, NONE);
    SimpleInstallFunction(console, "timeStamp", Builtins::kConsoleTimeStamp, 1,
                          false, NONE);
    JSObject::AddProperty(
        console, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("Object"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

#ifdef V8_INTL_SUPPORT
  {  // -- I n t l
    Handle<String> name = factory->InternalizeUtf8String("Intl");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
    Handle<JSObject> intl = factory->NewJSObject(cons, TENURED);
    DCHECK(intl->IsJSObject());
    JSObject::AddProperty(global, name, intl, DONT_ENUM);

    Handle<JSObject> date_time_format_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        date_time_format_prototype, factory->to_string_tag_symbol(),
        factory->Object_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
    Handle<JSFunction> date_time_format_constructor = InstallFunction(
        intl, "DateTimeFormat", JS_OBJECT_TYPE, DateFormat::kSize,
        date_time_format_prototype, Builtins::kIllegal);
    JSObject::AddProperty(date_time_format_prototype,
                          factory->constructor_string(),
                          date_time_format_constructor, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(
        isolate, date_time_format_constructor,
        Context::INTL_DATE_TIME_FORMAT_FUNCTION_INDEX);

    Handle<JSObject> number_format_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        number_format_prototype, factory->to_string_tag_symbol(),
        factory->Object_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
    Handle<JSFunction> number_format_constructor = InstallFunction(
        intl, "NumberFormat", JS_OBJECT_TYPE, NumberFormat::kSize,
        number_format_prototype, Builtins::kIllegal);
    JSObject::AddProperty(number_format_prototype,
                          factory->constructor_string(),
                          number_format_constructor, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(
        isolate, number_format_constructor,
        Context::INTL_NUMBER_FORMAT_FUNCTION_INDEX);

    Handle<JSObject> collator_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        collator_prototype, factory->to_string_tag_symbol(),
        factory->Object_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
    Handle<JSFunction> collator_constructor =
        InstallFunction(intl, "Collator", JS_OBJECT_TYPE, Collator::kSize,
                        collator_prototype, Builtins::kIllegal);
    JSObject::AddProperty(collator_prototype, factory->constructor_string(),
                          collator_constructor, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(isolate, collator_constructor,
                                     Context::INTL_COLLATOR_FUNCTION_INDEX);

    Handle<JSObject> v8_break_iterator_prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        v8_break_iterator_prototype, factory->to_string_tag_symbol(),
        factory->Object_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
    Handle<JSFunction> v8_break_iterator_constructor = InstallFunction(
        intl, "v8BreakIterator", JS_OBJECT_TYPE, V8BreakIterator::kSize,
        v8_break_iterator_prototype, Builtins::kIllegal);
    JSObject::AddProperty(v8_break_iterator_prototype,
                          factory->constructor_string(),
                          v8_break_iterator_constructor, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(
        isolate, v8_break_iterator_constructor,
        Context::INTL_V8_BREAK_ITERATOR_FUNCTION_INDEX);
  }
#endif  // V8_INTL_SUPPORT

  {  // -- A r r a y B u f f e r
    Handle<JSFunction> array_buffer_fun = InstallArrayBuffer(
        global, "ArrayBuffer", Builtins::kArrayBufferPrototypeGetByteLength,
        BuiltinFunctionId::kArrayBufferByteLength,
        Builtins::kArrayBufferPrototypeSlice);
    InstallWithIntrinsicDefaultProto(isolate, array_buffer_fun,
                                     Context::ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(array_buffer_fun);

    Handle<JSFunction> array_buffer_noinit_fun = SimpleCreateFunction(
        isolate,
        factory->NewStringFromAsciiChecked(
            "arrayBufferConstructor_DoNotInitialize"),
        Builtins::kArrayBufferConstructor_DoNotInitialize, 1, false);
    native_context()->set_array_buffer_noinit_fun(*array_buffer_noinit_fun);
  }

  {  // -- T y p e d A r r a y
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    native_context()->set_typed_array_prototype(*prototype);

    Handle<JSFunction> typed_array_fun =
        CreateFunction(isolate, factory->InternalizeUtf8String("TypedArray"),
                       JS_TYPED_ARRAY_TYPE, JSTypedArray::kSize, prototype,
                       Builtins::kIllegal);
    typed_array_fun->shared()->set_native(false);
    InstallSpeciesGetter(typed_array_fun);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(),
                          typed_array_fun, DONT_ENUM);
    native_context()->set_typed_array_function(*typed_array_fun);

    // Install the "buffer", "byteOffset", "byteLength" and "length"
    // getters on the {prototype}.
    SimpleInstallGetter(prototype, factory->buffer_string(),
                        Builtins::kTypedArrayPrototypeBuffer, false);
    SimpleInstallGetter(prototype, factory->byte_length_string(),
                        Builtins::kTypedArrayPrototypeByteLength, true,
                        kTypedArrayByteLength);
    SimpleInstallGetter(prototype, factory->byte_offset_string(),
                        Builtins::kTypedArrayPrototypeByteOffset, true,
                        kTypedArrayByteOffset);
    SimpleInstallGetter(prototype, factory->length_string(),
                        Builtins::kTypedArrayPrototypeLength, true,
                        kTypedArrayLength);

    // Install "keys", "values" and "entries" methods on the {prototype}.
    Handle<JSFunction> entries =
        SimpleInstallFunction(prototype, factory->entries_string(),
                              Builtins::kTypedArrayPrototypeEntries, 0, true);
    entries->shared()->set_builtin_function_id(kTypedArrayEntries);

    Handle<JSFunction> keys =
        SimpleInstallFunction(prototype, factory->keys_string(),
                              Builtins::kTypedArrayPrototypeKeys, 0, true);
    keys->shared()->set_builtin_function_id(kTypedArrayKeys);

    Handle<JSFunction> values =
        SimpleInstallFunction(prototype, factory->values_string(),
                              Builtins::kTypedArrayPrototypeValues, 0, true);
    values->shared()->set_builtin_function_id(kTypedArrayValues);
    JSObject::AddProperty(prototype, factory->iterator_symbol(), values,
                          DONT_ENUM);

    // TODO(caitp): alphasort accessors/methods
    SimpleInstallFunction(prototype, "copyWithin",
                          Builtins::kTypedArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(prototype, "every",
                          Builtins::kTypedArrayPrototypeEvery, 1, false);
    SimpleInstallFunction(prototype, "fill",
                          Builtins::kTypedArrayPrototypeFill, 1, false);
    SimpleInstallFunction(prototype, "includes",
                          Builtins::kTypedArrayPrototypeIncludes, 1, false);
    SimpleInstallFunction(prototype, "indexOf",
                          Builtins::kTypedArrayPrototypeIndexOf, 1, false);
    SimpleInstallFunction(prototype, "lastIndexOf",
                          Builtins::kTypedArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(prototype, "map", Builtins::kTypedArrayPrototypeMap,
                          1, false);
    SimpleInstallFunction(prototype, "reverse",
                          Builtins::kTypedArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(prototype, "reduce",
                          Builtins::kTypedArrayPrototypeReduce, 1, false);
    SimpleInstallFunction(prototype, "reduceRight",
                          Builtins::kTypedArrayPrototypeReduceRight, 1, false);
    SimpleInstallFunction(prototype, "slice",
                          Builtins::kTypedArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(prototype, "some", Builtins::kTypedArrayPrototypeSome,
                          1, false);
  }

  {  // -- T y p e d A r r a y s
#define INSTALL_TYPED_ARRAY(Type, type, TYPE, ctype, size)             \
  {                                                                    \
    Handle<JSFunction> fun;                                            \
    InstallTypedArray(#Type "Array", TYPE##_ELEMENTS, &fun);           \
    InstallWithIntrinsicDefaultProto(isolate, fun,                     \
                                     Context::TYPE##_ARRAY_FUN_INDEX); \
  }
    TYPED_ARRAYS(INSTALL_TYPED_ARRAY)
#undef INSTALL_TYPED_ARRAY

    // %typed_array_construct_by_length
    Handle<JSFunction> construct_by_length = SimpleCreateFunction(
        isolate,
        factory->NewStringFromAsciiChecked("typedArrayConstructByLength"),
        Builtins::kTypedArrayConstructByLength, 3, false);
    native_context()->set_typed_array_construct_by_length(*construct_by_length);

    // %typed_array_construct_by_array_buffer
    Handle<JSFunction> construct_by_buffer = SimpleCreateFunction(
        isolate,
        factory->NewStringFromAsciiChecked("typedArrayConstructByArrayBuffer"),
        Builtins::kTypedArrayConstructByArrayBuffer, 5, false);
    native_context()->set_typed_array_construct_by_array_buffer(
        *construct_by_buffer);

    // %typed_array_construct_by_array_like
    Handle<JSFunction> construct_by_array_like = SimpleCreateFunction(
        isolate,
        factory->NewStringFromAsciiChecked("typedArrayConstructByArrayLike"),
        Builtins::kTypedArrayConstructByArrayLike, 4, false);
    native_context()->set_typed_array_construct_by_array_like(
        *construct_by_array_like);
  }

  {  // -- D a t a V i e w
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> data_view_fun =
        InstallFunction(global, "DataView", JS_DATA_VIEW_TYPE,
                        JSDataView::kSizeWithEmbedderFields, prototype,
                        Builtins::kDataViewConstructor);
    InstallWithIntrinsicDefaultProto(isolate, data_view_fun,
                                     Context::DATA_VIEW_FUN_INDEX);
    data_view_fun->shared()->SetConstructStub(
        *isolate->builtins()->DataViewConstructor_ConstructStub());
    data_view_fun->shared()->set_length(3);
    data_view_fun->shared()->DontAdaptArguments();

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        prototype, factory->to_string_tag_symbol(),
        factory->NewStringFromAsciiChecked("DataView"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(),
                          data_view_fun, DONT_ENUM);

    // Install the "buffer", "byteOffset" and "byteLength" getters
    // on the {prototype}.
    SimpleInstallGetter(prototype, factory->buffer_string(),
                        Builtins::kDataViewPrototypeGetBuffer, false,
                        kDataViewBuffer);
    SimpleInstallGetter(prototype, factory->byte_length_string(),
                        Builtins::kDataViewPrototypeGetByteLength, false,
                        kDataViewByteLength);
    SimpleInstallGetter(prototype, factory->byte_offset_string(),
                        Builtins::kDataViewPrototypeGetByteOffset, false,
                        kDataViewByteOffset);

    SimpleInstallFunction(prototype, "getInt8",
                          Builtins::kDataViewPrototypeGetInt8, 1, false);
    SimpleInstallFunction(prototype, "setInt8",
                          Builtins::kDataViewPrototypeSetInt8, 2, false);
    SimpleInstallFunction(prototype, "getUint8",
                          Builtins::kDataViewPrototypeGetUint8, 1, false);
    SimpleInstallFunction(prototype, "setUint8",
                          Builtins::kDataViewPrototypeSetUint8, 2, false);
    SimpleInstallFunction(prototype, "getInt16",
                          Builtins::kDataViewPrototypeGetInt16, 1, false);
    SimpleInstallFunction(prototype, "setInt16",
                          Builtins::kDataViewPrototypeSetInt16, 2, false);
    SimpleInstallFunction(prototype, "getUint16",
                          Builtins::kDataViewPrototypeGetUint16, 1, false);
    SimpleInstallFunction(prototype, "setUint16",
                          Builtins::kDataViewPrototypeSetUint16, 2, false);
    SimpleInstallFunction(prototype, "getInt32",
                          Builtins::kDataViewPrototypeGetInt32, 1, false);
    SimpleInstallFunction(prototype, "setInt32",
                          Builtins::kDataViewPrototypeSetInt32, 2, false);
    SimpleInstallFunction(prototype, "getUint32",
                          Builtins::kDataViewPrototypeGetUint32, 1, false);
    SimpleInstallFunction(prototype, "setUint32",
                          Builtins::kDataViewPrototypeSetUint32, 2, false);
    SimpleInstallFunction(prototype, "getFloat32",
                          Builtins::kDataViewPrototypeGetFloat32, 1, false);
    SimpleInstallFunction(prototype, "setFloat32",
                          Builtins::kDataViewPrototypeSetFloat32, 2, false);
    SimpleInstallFunction(prototype, "getFloat64",
                          Builtins::kDataViewPrototypeGetFloat64, 1, false);
    SimpleInstallFunction(prototype, "setFloat64",
                          Builtins::kDataViewPrototypeSetFloat64, 2, false);
  }

  {  // -- M a p
    Handle<JSFunction> js_map_fun = InstallFunction(
        global, "Map", JS_MAP_TYPE, JSMap::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_map_fun,
                                     Context::JS_MAP_FUN_INDEX);
    InstallSpeciesGetter(js_map_fun);
  }

  {  // -- S e t
    Handle<JSFunction> js_set_fun = InstallFunction(
        global, "Set", JS_SET_TYPE, JSSet::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_set_fun,
                                     Context::JS_SET_FUN_INDEX);
    InstallSpeciesGetter(js_set_fun);
  }

  {  // -- J S M o d u l e N a m e s p a c e
    Handle<Map> map =
        factory->NewMap(JS_MODULE_NAMESPACE_TYPE, JSModuleNamespace::kSize);
    Map::SetPrototype(map, isolate->factory()->null_value());
    Map::EnsureDescriptorSlack(map, 1);
    native_context()->set_js_module_namespace_map(*map);

    {  // Install @@toStringTag.
      PropertyAttributes attribs =
          static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
      Descriptor d =
          Descriptor::DataField(factory->to_string_tag_symbol(),
                                JSModuleNamespace::kToStringTagFieldIndex,
                                attribs, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    map->SetInObjectProperties(JSModuleNamespace::kInObjectFieldCount);
  }

  {  // -- I t e r a t o r R e s u l t
    Handle<Map> map =
        factory->NewMap(JS_OBJECT_TYPE, JSIteratorResult::kSize);
    Map::SetPrototype(map, isolate->initial_object_prototype());
    Map::EnsureDescriptorSlack(map, 2);

    {  // value
      Descriptor d = Descriptor::DataField(factory->value_string(),
                                           JSIteratorResult::kValueIndex, NONE,
                                           Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    {  // done
      Descriptor d = Descriptor::DataField(factory->done_string(),
                                           JSIteratorResult::kDoneIndex, NONE,
                                           Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    map->SetConstructor(native_context()->object_function());
    map->SetInObjectProperties(2);
    native_context()->set_iterator_result_map(*map);
  }

  {  // -- W e a k M a p
    Handle<JSFunction> js_weak_map_fun = InstallFunction(
        global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_weak_map_fun,
                                     Context::JS_WEAK_MAP_FUN_INDEX);
  }

  {  // -- W e a k S e t
    Handle<JSFunction> js_weak_set_fun = InstallFunction(
        global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_weak_set_fun,
                                     Context::JS_WEAK_SET_FUN_INDEX);
  }

  {  // -- P r o x y
    CreateJSProxyMaps();

    Handle<String> name = factory->Proxy_string();
    Handle<Code> code(isolate->builtins()->ProxyConstructor());

    Handle<JSFunction> proxy_function =
        factory->NewFunction(isolate->proxy_function_map(),
                             factory->Proxy_string(), MaybeHandle<Code>(code));

    JSFunction::SetInitialMap(
        proxy_function, Handle<Map>(native_context()->proxy_map(), isolate),
        factory->null_value());

    proxy_function->shared()->SetConstructStub(
        *isolate->builtins()->ProxyConstructor_ConstructStub());
    proxy_function->shared()->set_internal_formal_parameter_count(2);
    proxy_function->shared()->set_length(2);

    native_context()->set_proxy_function(*proxy_function);
    InstallFunction(global, name, proxy_function, factory->Object_string());
  }

  {  // -- R e f l e c t
    Handle<String> reflect_string = factory->InternalizeUtf8String("Reflect");
    Handle<JSObject> reflect =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSObject::AddProperty(global, reflect_string, reflect, DONT_ENUM);

    Handle<JSFunction> define_property =
        SimpleInstallFunction(reflect, factory->defineProperty_string(),
                              Builtins::kReflectDefineProperty, 3, true);
    native_context()->set_reflect_define_property(*define_property);

    Handle<JSFunction> delete_property =
        SimpleInstallFunction(reflect, factory->deleteProperty_string(),
                              Builtins::kReflectDeleteProperty, 2, true);
    native_context()->set_reflect_delete_property(*delete_property);

    Handle<JSFunction> apply = SimpleInstallFunction(
        reflect, factory->apply_string(), Builtins::kReflectApply, 3, false);
    native_context()->set_reflect_apply(*apply);

    Handle<JSFunction> construct =
        SimpleInstallFunction(reflect, factory->construct_string(),
                              Builtins::kReflectConstruct, 2, false);
    native_context()->set_reflect_construct(*construct);

    SimpleInstallFunction(reflect, factory->get_string(), Builtins::kReflectGet,
                          2, false);
    SimpleInstallFunction(reflect, factory->getOwnPropertyDescriptor_string(),
                          Builtins::kReflectGetOwnPropertyDescriptor, 2, true);
    SimpleInstallFunction(reflect, factory->getPrototypeOf_string(),
                          Builtins::kReflectGetPrototypeOf, 1, true);
    SimpleInstallFunction(reflect, factory->has_string(), Builtins::kReflectHas,
                          2, true);
    SimpleInstallFunction(reflect, factory->isExtensible_string(),
                          Builtins::kReflectIsExtensible, 1, true);
    SimpleInstallFunction(reflect, factory->ownKeys_string(),
                          Builtins::kReflectOwnKeys, 1, true);
    SimpleInstallFunction(reflect, factory->preventExtensions_string(),
                          Builtins::kReflectPreventExtensions, 1, true);
    SimpleInstallFunction(reflect, factory->set_string(), Builtins::kReflectSet,
                          3, false);
    SimpleInstallFunction(reflect, factory->setPrototypeOf_string(),
                          Builtins::kReflectSetPrototypeOf, 2, true);
  }

  {  // --- B o u n d F u n c t i o n
    Handle<Map> map =
        factory->NewMap(JS_BOUND_FUNCTION_TYPE, JSBoundFunction::kSize);
    map->set_is_callable();
    Map::SetPrototype(map, empty_function);

    PropertyAttributes roc_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    Map::EnsureDescriptorSlack(map, 2);

    Handle<AccessorInfo> bound_length =
        Accessors::BoundFunctionLengthInfo(isolate, roc_attribs);
    {  // length
      Descriptor d = Descriptor::AccessorConstant(factory->length_string(),
                                                  bound_length, roc_attribs);
      map->AppendDescriptor(&d);
    }
    Handle<AccessorInfo> bound_name =
        Accessors::BoundFunctionNameInfo(isolate, roc_attribs);
    {  // name
      Descriptor d = Descriptor::AccessorConstant(factory->name_string(),
                                                  bound_name, roc_attribs);
      map->AppendDescriptor(&d);
    }
    map->SetInObjectProperties(0);
    native_context()->set_bound_function_without_constructor_map(*map);

    map = Map::Copy(map, "IsConstructor");
    map->set_is_constructor(true);
    native_context()->set_bound_function_with_constructor_map(*map);
  }

  {  // --- sloppy arguments map
    // Make sure we can recognize argument objects at runtime.
    // This is done by introducing an anonymous function with
    // class_name equals 'Arguments'.
    Handle<String> arguments_string = factory->Arguments_string();
    Handle<Code> code = isolate->builtins()->Illegal();
    Handle<JSFunction> function = factory->NewFunctionWithoutPrototype(
        arguments_string, code);
    function->shared()->set_instance_class_name(*arguments_string);

    Handle<Map> map = factory->NewMap(
        JS_ARGUMENTS_TYPE, JSSloppyArgumentsObject::kSize, FAST_ELEMENTS);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 2);

    {  // length
      Descriptor d = Descriptor::DataField(
          factory->length_string(), JSSloppyArgumentsObject::kLengthIndex,
          DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      Descriptor d = Descriptor::DataField(
          factory->callee_string(), JSSloppyArgumentsObject::kCalleeIndex,
          DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    // @@iterator method is added later.

    map->SetInObjectProperties(2);
    native_context()->set_sloppy_arguments_map(*map);

    DCHECK(!function->has_initial_map());
    JSFunction::SetInitialMap(function, map,
                              isolate->initial_object_prototype());

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsFastObjectElementsKind(map->elements_kind()));
  }

  {  // --- fast and slow aliased arguments map
    Handle<Map> map = isolate->sloppy_arguments_map();
    map = Map::Copy(map, "FastAliasedArguments");
    map->set_elements_kind(FAST_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_fast_aliased_arguments_map(*map);

    map = Map::Copy(map, "SlowAliasedArguments");
    map->set_elements_kind(SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_slow_aliased_arguments_map(*map);
  }

  {  // --- strict mode arguments map
    const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError function.
    Handle<AccessorPair> callee = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetStrictArgumentsPoisonFunction();

    // Install the ThrowTypeError function.
    callee->set_getter(*poison);
    callee->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(
        JS_ARGUMENTS_TYPE, JSStrictArgumentsObject::kSize, FAST_ELEMENTS);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 2);

    {  // length
      Descriptor d = Descriptor::DataField(
          factory->length_string(), JSStrictArgumentsObject::kLengthIndex,
          DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      Descriptor d = Descriptor::AccessorConstant(factory->callee_string(),
                                                  callee, attributes);
      map->AppendDescriptor(&d);
    }
    // @@iterator method is added later.

    DCHECK_EQ(native_context()->object_function()->prototype(),
              *isolate->initial_object_prototype());
    Map::SetPrototype(map, isolate->initial_object_prototype());
    map->SetInObjectProperties(1);

    // Copy constructor from the sloppy arguments boilerplate.
    map->SetConstructor(
        native_context()->sloppy_arguments_map()->GetConstructor());

    native_context()->set_strict_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsFastObjectElementsKind(map->elements_kind()));
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<Code> code = isolate->builtins()->Illegal();
    Handle<JSFunction> context_extension_fun = factory->NewFunction(
        factory->empty_string(), code, JS_CONTEXT_EXTENSION_OBJECT_TYPE,
        JSObject::kHeaderSize);

    Handle<String> name = factory->InternalizeOneByteString(
        STATIC_CHAR_VECTOR("context_extension"));
    context_extension_fun->shared()->set_instance_class_name(*name);
    native_context()->set_context_extension_function(*context_extension_fun);
  }


  {
    // Set up the call-as-function delegate.
    Handle<Code> code = isolate->builtins()->HandleApiCallAsFunction();
    Handle<JSFunction> delegate = factory->NewFunction(
        factory->empty_string(), code, JS_OBJECT_TYPE, JSObject::kHeaderSize);
    native_context()->set_call_as_function_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<Code> code = isolate->builtins()->HandleApiCallAsConstructor();
    Handle<JSFunction> delegate = factory->NewFunction(
        factory->empty_string(), code, JS_OBJECT_TYPE, JSObject::kHeaderSize);
    native_context()->set_call_as_constructor_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }
}  // NOLINT(readability/fn_size)

void Genesis::InstallTypedArray(const char* name, ElementsKind elements_kind,
                                Handle<JSFunction>* fun) {
  Handle<JSObject> global = Handle<JSObject>(native_context()->global_object());

  Handle<JSObject> typed_array_prototype =
      Handle<JSObject>(isolate()->typed_array_prototype());
  Handle<JSFunction> typed_array_function =
      Handle<JSFunction>(isolate()->typed_array_function());

  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSFunction> result = InstallFunction(
      global, name, JS_TYPED_ARRAY_TYPE, JSTypedArray::kSizeWithEmbedderFields,
      prototype, Builtins::kIllegal);
  result->initial_map()->set_elements_kind(elements_kind);

  CHECK(JSObject::SetPrototype(result, typed_array_function, false,
                               Object::DONT_THROW)
            .FromJust());

  CHECK(JSObject::SetPrototype(prototype, typed_array_prototype, false,
                               Object::DONT_THROW)
            .FromJust());
  *fun = result;
}


void Genesis::InitializeExperimentalGlobal() {
#define FEATURE_INITIALIZE_GLOBAL(id, descr) InitializeGlobal_##id();

  HARMONY_INPROGRESS(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_STAGED(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_SHIPPING(FEATURE_INITIALIZE_GLOBAL)
#undef FEATURE_INITIALIZE_GLOBAL
}

bool Bootstrapper::CompileBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(CORE, index);

  // We pass in extras_utils so that builtin code can set it up for later use
  // by actual extras code, compiled with CompileExtraBuiltin.
  Handle<Object> global = isolate->global_object();
  Handle<Object> utils = isolate->natives_utils_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, utils, extras_utils};

  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, NATIVES_CODE);
}


bool Bootstrapper::CompileExtraBuiltin(Isolate* isolate, int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExtraNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(EXTRAS, index);
  Handle<Object> global = isolate->global_object();
  Handle<Object> binding = isolate->extras_binding_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, binding, extras_utils};
  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, EXTENSION_CODE);
}


bool Bootstrapper::CompileExperimentalExtraBuiltin(Isolate* isolate,
                                                   int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExperimentalExtraNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(EXPERIMENTAL_EXTRAS, index);
  Handle<Object> global = isolate->global_object();
  Handle<Object> binding = isolate->extras_binding_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, binding, extras_utils};
  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, EXTENSION_CODE);
}

bool Bootstrapper::CompileNative(Isolate* isolate, Vector<const char> name,
                                 Handle<String> source, int argc,
                                 Handle<Object> argv[],
                                 NativesFlag natives_flag) {
  SuppressDebug compiling_natives(isolate->debug());
  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(4 * KB)) {
    isolate->StackOverflow();
    return false;
  }

  Handle<Context> context(isolate->context());

  Handle<String> script_name =
      isolate->factory()->NewStringFromUtf8(name).ToHandleChecked();
  Handle<SharedFunctionInfo> function_info =
      Compiler::GetSharedFunctionInfoForScript(
          source, script_name, 0, 0, ScriptOriginOptions(), Handle<Object>(),
          context, NULL, NULL, ScriptCompiler::kNoCompileOptions, natives_flag);
  if (function_info.is_null()) return false;

  DCHECK(context->IsNativeContext());

  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(function_info,
                                                            context);
  Handle<Object> receiver = isolate->factory()->undefined_value();

  // For non-extension scripts, run script to get the function wrapper.
  Handle<Object> wrapper;
  if (!Execution::TryCall(isolate, fun, receiver, 0, nullptr,
                          Execution::MessageHandling::kKeepPending, nullptr)
           .ToHandle(&wrapper)) {
    return false;
  }
  // Then run the function wrapper.
  return !Execution::TryCall(isolate, Handle<JSFunction>::cast(wrapper),
                             receiver, argc, argv,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}


bool Genesis::CallUtilsFunction(Isolate* isolate, const char* name) {
  Handle<JSObject> utils =
      Handle<JSObject>::cast(isolate->natives_utils_object());
  Handle<String> name_string =
      isolate->factory()->NewStringFromAsciiChecked(name);
  Handle<Object> fun = JSObject::GetDataProperty(utils, name_string);
  Handle<Object> receiver = isolate->factory()->undefined_value();
  Handle<Object> args[] = {utils};
  return !Execution::TryCall(isolate, fun, receiver, 1, args,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
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
  Handle<Context> context(isolate->context());
  DCHECK(context->IsNativeContext());

  if (!cache->Lookup(name, &function_info)) {
    Handle<String> script_name =
        factory->NewStringFromUtf8(name).ToHandleChecked();
    function_info = Compiler::GetSharedFunctionInfoForScript(
        source, script_name, 0, 0, ScriptOriginOptions(), Handle<Object>(),
        context, extension, NULL, ScriptCompiler::kNoCompileOptions,
        EXTENSION_CODE);
    if (function_info.is_null()) return false;
    cache->Add(name, function_info);
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


static Handle<JSObject> ResolveBuiltinIdHolder(Handle<Context> native_context,
                                               const char* holder_expr) {
  Isolate* isolate = native_context->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<JSGlobalObject> global(native_context->global_object());
  const char* period_pos = strchr(holder_expr, '.');
  if (period_pos == NULL) {
    return Handle<JSObject>::cast(
        Object::GetPropertyOrElement(
            global, factory->InternalizeUtf8String(holder_expr))
            .ToHandleChecked());
  }
  const char* inner = period_pos + 1;
  DCHECK(!strchr(inner, '.'));
  Vector<const char> property(holder_expr,
                              static_cast<int>(period_pos - holder_expr));
  Handle<String> property_string = factory->InternalizeUtf8String(property);
  DCHECK(!property_string.is_null());
  Handle<JSObject> object = Handle<JSObject>::cast(
      JSReceiver::GetProperty(global, property_string).ToHandleChecked());
  if (strcmp("prototype", inner) == 0) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(object);
    return Handle<JSObject>(JSObject::cast(function->prototype()));
  }
  Handle<String> inner_string = factory->InternalizeUtf8String(inner);
  DCHECK(!inner_string.is_null());
  Handle<Object> value =
      JSReceiver::GetProperty(object, inner_string).ToHandleChecked();
  return Handle<JSObject>::cast(value);
}

void Genesis::ConfigureUtilsObject(GlobalContextType context_type) {
  switch (context_type) {
    // We still need the utils object to find debug functions.
    case DEBUG_CONTEXT:
      return;
    // Expose the natives in global if a valid name for it is specified.
    case FULL_CONTEXT: {
      // We still need the utils object after deserialization.
      if (isolate()->serializer_enabled()) return;
      if (FLAG_expose_natives_as == NULL) break;
      if (strlen(FLAG_expose_natives_as) == 0) break;
      HandleScope scope(isolate());
      Handle<String> natives_key =
          factory()->InternalizeUtf8String(FLAG_expose_natives_as);
      uint32_t dummy_index;
      if (natives_key->AsArrayIndex(&dummy_index)) break;
      Handle<Object> utils = isolate()->natives_utils_object();
      Handle<JSObject> global = isolate()->global_object();
      JSObject::AddProperty(global, natives_key, utils, DONT_ENUM);
      break;
    }
  }

  // The utils object can be removed for cases that reach this point.
  native_context()->set_natives_utils_object(heap()->undefined_value());
  native_context()->set_extras_utils_object(heap()->undefined_value());
  native_context()->set_exports_container(heap()->undefined_value());
}


void Bootstrapper::ExportFromRuntime(Isolate* isolate,
                                     Handle<JSObject> container) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<Context> native_context = isolate->native_context();
#define EXPORT_PRIVATE_SYMBOL(NAME)                                       \
  Handle<String> NAME##_name = factory->NewStringFromAsciiChecked(#NAME); \
  JSObject::AddProperty(container, NAME##_name, factory->NAME(), NONE);
  PRIVATE_SYMBOL_LIST(EXPORT_PRIVATE_SYMBOL)
#undef EXPORT_PRIVATE_SYMBOL

#define EXPORT_PUBLIC_SYMBOL(NAME, DESCRIPTION)                           \
  Handle<String> NAME##_name = factory->NewStringFromAsciiChecked(#NAME); \
  JSObject::AddProperty(container, NAME##_name, factory->NAME(), NONE);
  PUBLIC_SYMBOL_LIST(EXPORT_PUBLIC_SYMBOL)
  WELL_KNOWN_SYMBOL_LIST(EXPORT_PUBLIC_SYMBOL)
#undef EXPORT_PUBLIC_SYMBOL

  Handle<JSObject> iterator_prototype(
      native_context->initial_iterator_prototype());

  JSObject::AddProperty(container,
                        factory->InternalizeUtf8String("IteratorPrototype"),
                        iterator_prototype, NONE);

  {
    PrototypeIterator iter(native_context->generator_function_map());
    Handle<JSObject> generator_function_prototype(iter.GetCurrent<JSObject>());

    JSObject::AddProperty(
        container, factory->InternalizeUtf8String("GeneratorFunctionPrototype"),
        generator_function_prototype, NONE);

    static const bool kUseStrictFunctionMap = true;
    Handle<JSFunction> generator_function_function = InstallFunction(
        container, "GeneratorFunction", JS_FUNCTION_TYPE, JSFunction::kSize,
        generator_function_prototype, Builtins::kGeneratorFunctionConstructor,
        kUseStrictFunctionMap);
    generator_function_function->set_prototype_or_initial_map(
        native_context->generator_function_map());
    generator_function_function->shared()->DontAdaptArguments();
    generator_function_function->shared()->SetConstructStub(
        *isolate->builtins()->GeneratorFunctionConstructor());
    generator_function_function->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, generator_function_function,
        Context::GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(generator_function_function,
                                isolate->function_function());
    JSObject::AddProperty(
        generator_function_prototype, factory->constructor_string(),
        generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->generator_function_map()->SetConstructor(
        *generator_function_function);
  }

  {
    PrototypeIterator iter(native_context->async_generator_function_map());
    Handle<JSObject> async_generator_function_prototype(
        iter.GetCurrent<JSObject>());

    static const bool kUseStrictFunctionMap = true;
    Handle<JSFunction> async_generator_function_function = InstallFunction(
        container, "AsyncGeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSize, async_generator_function_prototype,
        Builtins::kAsyncGeneratorFunctionConstructor, kUseStrictFunctionMap);
    async_generator_function_function->set_prototype_or_initial_map(
        native_context->async_generator_function_map());
    async_generator_function_function->shared()->DontAdaptArguments();
    async_generator_function_function->shared()->SetConstructStub(
        *isolate->builtins()->AsyncGeneratorFunctionConstructor());
    async_generator_function_function->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, async_generator_function_function,
        Context::ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(async_generator_function_function,
                                isolate->function_function());

    JSObject::AddProperty(
        async_generator_function_prototype, factory->constructor_string(),
        async_generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->async_generator_function_map()->SetConstructor(
        *async_generator_function_function);
  }

  {  // -- S e t I t e r a t o r
    Handle<JSObject> set_iterator_prototype =
        isolate->factory()->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(set_iterator_prototype, iterator_prototype);
    Handle<JSFunction> set_iterator_function = InstallFunction(
        container, "SetIterator", JS_SET_ITERATOR_TYPE, JSSetIterator::kSize,
        set_iterator_prototype, Builtins::kIllegal);
    native_context->set_set_iterator_map(set_iterator_function->initial_map());
  }

  {  // -- M a p I t e r a t o r
    Handle<JSObject> map_iterator_prototype =
        isolate->factory()->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(map_iterator_prototype, iterator_prototype);
    Handle<JSFunction> map_iterator_function = InstallFunction(
        container, "MapIterator", JS_MAP_ITERATOR_TYPE, JSMapIterator::kSize,
        map_iterator_prototype, Builtins::kIllegal);
    native_context->set_map_iterator_map(map_iterator_function->initial_map());
  }

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun = InstallFunction(
        container, "Script", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kUnsupportedThrower);
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSFunction::SetPrototype(script_fun, prototype);
    native_context->set_script_function(*script_fun);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());
    Map::EnsureDescriptorSlack(script_map, 15);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    Handle<AccessorInfo> script_column =
        Accessors::ScriptColumnOffsetInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_column->name())), script_column,
          attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_id = Accessors::ScriptIdInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_id->name())), script_id, attribs);
      script_map->AppendDescriptor(&d);
    }


    Handle<AccessorInfo> script_name =
        Accessors::ScriptNameInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_name->name())), script_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_line =
        Accessors::ScriptLineOffsetInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_line->name())), script_line, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source =
        Accessors::ScriptSourceInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_source->name())), script_source,
          attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_type =
        Accessors::ScriptTypeInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_type->name())), script_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_compilation_type =
        Accessors::ScriptCompilationTypeInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_compilation_type->name())),
          script_compilation_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_context_data =
        Accessors::ScriptContextDataInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_context_data->name())),
          script_context_data, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script =
        Accessors::ScriptEvalFromScriptInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_eval_from_script->name())),
          script_eval_from_script, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script_position =
        Accessors::ScriptEvalFromScriptPositionInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_eval_from_script_position->name())),
          script_eval_from_script_position, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_function_name =
        Accessors::ScriptEvalFromFunctionNameInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_eval_from_function_name->name())),
          script_eval_from_function_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_url =
        Accessors::ScriptSourceUrlInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_source_url->name())),
          script_source_url, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_mapping_url =
        Accessors::ScriptSourceMappingUrlInfo(isolate, attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(
          Handle<Name>(Name::cast(script_source_mapping_url->name())),
          script_source_mapping_url, attribs);
      script_map->AppendDescriptor(&d);
    }
  }

  {  // -- A s y n c F u n c t i o n
    // Builtin functions for AsyncFunction.
    PrototypeIterator iter(native_context->async_function_map());
    Handle<JSObject> async_function_prototype(iter.GetCurrent<JSObject>());

    static const bool kUseStrictFunctionMap = true;
    Handle<JSFunction> async_function_constructor = InstallFunction(
        container, "AsyncFunction", JS_FUNCTION_TYPE, JSFunction::kSize,
        async_function_prototype, Builtins::kAsyncFunctionConstructor,
        kUseStrictFunctionMap);
    async_function_constructor->shared()->DontAdaptArguments();
    async_function_constructor->shared()->SetConstructStub(
        *isolate->builtins()->AsyncFunctionConstructor());
    async_function_constructor->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, async_function_constructor,
                                     Context::ASYNC_FUNCTION_FUNCTION_INDEX);
    JSObject::ForceSetPrototype(async_function_constructor,
                                isolate->function_function());

    JSObject::AddProperty(
        async_function_prototype, factory->constructor_string(),
        async_function_constructor,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    JSFunction::SetPrototype(async_function_constructor,
                             async_function_prototype);

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionAwaitCaught, 3, false);
      InstallWithIntrinsicDefaultProto(
          isolate, function, Context::ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX);
    }

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionAwaitUncaught, 3, false);
      InstallWithIntrinsicDefaultProto(
          isolate, function, Context::ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX);
    }

    {
      Handle<Code> code =
          isolate->builtins()->AsyncFunctionAwaitRejectClosure();
      Handle<SharedFunctionInfo> info =
          factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
      info->set_internal_formal_parameter_count(1);
      info->set_length(1);
      native_context->set_async_function_await_reject_shared_fun(*info);
    }

    {
      Handle<Code> code =
          isolate->builtins()->AsyncFunctionAwaitResolveClosure();
      Handle<SharedFunctionInfo> info =
          factory->NewSharedFunctionInfo(factory->empty_string(), code, false);
      info->set_internal_formal_parameter_count(1);
      info->set_length(1);
      native_context->set_async_function_await_resolve_shared_fun(*info);
    }

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionPromiseCreate, 0, false);
      InstallWithIntrinsicDefaultProto(
          isolate, function, Context::ASYNC_FUNCTION_PROMISE_CREATE_INDEX);
    }

    {
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate, factory->empty_string(),
          Builtins::kAsyncFunctionPromiseRelease, 1, false);
      InstallWithIntrinsicDefaultProto(
          isolate, function, Context::ASYNC_FUNCTION_PROMISE_RELEASE_INDEX);
    }
  }

  {  // -- C a l l S i t e
    // Builtin functions for CallSite.

    // CallSites are a special case; the constructor is for our private use
    // only, therefore we set it up as a builtin that throws. Internally, we use
    // CallSiteUtils::Construct to create CallSite objects.

    Handle<JSFunction> callsite_fun = InstallFunction(
        container, "CallSite", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kUnsupportedThrower);
    callsite_fun->shared()->DontAdaptArguments();
    isolate->native_context()->set_callsite_function(*callsite_fun);

    {
      Handle<JSObject> proto =
          factory->NewJSObject(isolate->object_function(), TENURED);
      JSObject::AddProperty(proto, factory->constructor_string(), callsite_fun,
                            DONT_ENUM);

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
          {"getScriptNameOrSourceURL",
           Builtins::kCallSitePrototypeGetScriptNameOrSourceURL},
          {"getThis", Builtins::kCallSitePrototypeGetThis},
          {"getTypeName", Builtins::kCallSitePrototypeGetTypeName},
          {"isConstructor", Builtins::kCallSitePrototypeIsConstructor},
          {"isEval", Builtins::kCallSitePrototypeIsEval},
          {"isNative", Builtins::kCallSitePrototypeIsNative},
          {"isToplevel", Builtins::kCallSitePrototypeIsToplevel},
          {"toString", Builtins::kCallSitePrototypeToString}};

      PropertyAttributes attrs =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

      Handle<JSFunction> fun;
      for (const FunctionInfo& info : infos) {
        SimpleInstallFunction(proto, info.name, info.id, 0, true, attrs);
      }

      JSFunction::SetPrototype(callsite_fun, proto);
    }
  }
  isolate->native_context()->set_exports_container(*container);
}


#define EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(id) \
  void Genesis::InitializeGlobal_##id() {}

EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_do_expressions)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_lookbehind)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_named_captures)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_property)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_function_sent)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_tailcalls)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_restrictive_generators)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_trailing_commas)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_function_tostring)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_class_fields)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_object_rest_spread)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_dynamic_import)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_template_escapes)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_restrict_constructor_return)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_strict_legacy_accessor_builtins)

void InstallPublicSymbol(Factory* factory, Handle<Context> native_context,
                         const char* name, Handle<Symbol> value) {
  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(native_context->global_object()));
  Handle<String> symbol_string = factory->InternalizeUtf8String("Symbol");
  Handle<JSObject> symbol = Handle<JSObject>::cast(
      JSObject::GetProperty(global, symbol_string).ToHandleChecked());
  Handle<String> name_string = factory->InternalizeUtf8String(name);
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  JSObject::AddProperty(symbol, name_string, value, attributes);
}

void Genesis::InstallOneBuiltinFunction(Handle<Object> prototype,
                                        const char* method_name,
                                        Builtins::Name builtin_name) {
  LookupIterator it(
      prototype, isolate()->factory()->NewStringFromAsciiChecked(method_name),
      LookupIterator::OWN_SKIP_INTERCEPTOR);
  Handle<Object> function = Object::GetProperty(&it).ToHandleChecked();
  Handle<JSFunction>::cast(function)->set_code(
      isolate()->builtins()->builtin(builtin_name));
  SharedFunctionInfo* info = Handle<JSFunction>::cast(function)->shared();
  info->set_code(isolate()->builtins()->builtin(builtin_name));
  info->set_internal_formal_parameter_count(
      Builtins::GetBuiltinParameterCount(builtin_name));
}

void Genesis::InitializeGlobal_harmony_sharedarraybuffer() {
  if (!FLAG_harmony_sharedarraybuffer) return;

  Handle<JSGlobalObject> global(native_context()->global_object());
  Isolate* isolate = global->GetIsolate();
  Factory* factory = isolate->factory();

  Handle<JSFunction> shared_array_buffer_fun =
      InstallArrayBuffer(global, "SharedArrayBuffer",
                         Builtins::kSharedArrayBufferPrototypeGetByteLength,
                         BuiltinFunctionId::kSharedArrayBufferByteLength,
                         Builtins::kSharedArrayBufferPrototypeSlice);
  InstallWithIntrinsicDefaultProto(isolate, shared_array_buffer_fun,
                                   Context::SHARED_ARRAY_BUFFER_FUN_INDEX);
  InstallSpeciesGetter(shared_array_buffer_fun);

  Handle<String> name = factory->InternalizeUtf8String("Atomics");
  Handle<JSFunction> cons = factory->NewFunction(name);
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
  Handle<JSObject> atomics_object = factory->NewJSObject(cons, TENURED);
  DCHECK(atomics_object->IsJSObject());
  JSObject::AddProperty(global, name, atomics_object, DONT_ENUM);
  JSObject::AddProperty(atomics_object, factory->to_string_tag_symbol(), name,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("load"),
                        Builtins::kAtomicsLoad, 2, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("store"),
                        Builtins::kAtomicsStore, 3, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("add"),
                        Builtins::kAtomicsAdd, 3, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("sub"),
                        Builtins::kAtomicsSub, 3, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("and"),
                        Builtins::kAtomicsAnd, 3, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("or"),
                        Builtins::kAtomicsOr, 3, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("xor"),
                        Builtins::kAtomicsXor, 3, true);
  SimpleInstallFunction(atomics_object,
                        factory->InternalizeUtf8String("exchange"),
                        Builtins::kAtomicsExchange, 3, true);
  SimpleInstallFunction(atomics_object,
                        factory->InternalizeUtf8String("compareExchange"),
                        Builtins::kAtomicsCompareExchange, 4, true);
  SimpleInstallFunction(atomics_object,
                        factory->InternalizeUtf8String("isLockFree"),
                        Builtins::kAtomicsIsLockFree, 1, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("wait"),
                        Builtins::kAtomicsWait, 4, true);
  SimpleInstallFunction(atomics_object, factory->InternalizeUtf8String("wake"),
                        Builtins::kAtomicsWake, 3, true);
}

void Genesis::InitializeGlobal_harmony_array_prototype_values() {
  if (!FLAG_harmony_array_prototype_values) return;
  Handle<JSFunction> array_constructor(native_context()->array_function());
  Handle<JSObject> array_prototype(
      JSObject::cast(array_constructor->instance_prototype()));
  Handle<Object> values_iterator =
      JSObject::GetProperty(array_prototype, factory()->iterator_symbol())
          .ToHandleChecked();
  DCHECK(values_iterator->IsJSFunction());
  JSObject::AddProperty(array_prototype, factory()->values_string(),
                        values_iterator, DONT_ENUM);

  Handle<Object> unscopables =
      JSObject::GetProperty(array_prototype, factory()->unscopables_symbol())
          .ToHandleChecked();
  DCHECK(unscopables->IsJSObject());
  JSObject::AddProperty(Handle<JSObject>::cast(unscopables),
                        factory()->values_string(), factory()->true_value(),
                        NONE);
}

void Genesis::InitializeGlobal_harmony_async_iteration() {
  if (!FLAG_harmony_async_iteration) return;
  Handle<JSFunction> symbol_fun(native_context()->symbol_function());
  InstallConstant(isolate(), symbol_fun, "asyncIterator",
                  factory()->async_iterator_symbol());
}

void Genesis::InitializeGlobal_harmony_promise_finally() {
  if (!FLAG_harmony_promise_finally) return;

  Handle<JSFunction> constructor(native_context()->promise_function());
  Handle<JSObject> prototype(JSObject::cast(constructor->instance_prototype()));
  SimpleInstallFunction(prototype, "finally", Builtins::kPromiseFinally, 1,
                        true, DONT_ENUM);

  // The promise prototype map has changed because we added a property
  // to prototype, so we update the saved map.
  Handle<Map> prototype_map(prototype->map());
  Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate());
  native_context()->set_promise_prototype_map(*prototype_map);

  {
    Handle<Code> code =
        handle(isolate()->builtins()->builtin(Builtins::kPromiseThenFinally),
               isolate());
    Handle<SharedFunctionInfo> info = factory()->NewSharedFunctionInfo(
        factory()->empty_string(), code, false);
    info->set_internal_formal_parameter_count(1);
    info->set_length(1);
    info->set_native(true);
    native_context()->set_promise_then_finally_shared_fun(*info);
  }

  {
    Handle<Code> code =
        handle(isolate()->builtins()->builtin(Builtins::kPromiseCatchFinally),
               isolate());
    Handle<SharedFunctionInfo> info = factory()->NewSharedFunctionInfo(
        factory()->empty_string(), code, false);
    info->set_internal_formal_parameter_count(1);
    info->set_length(1);
    info->set_native(true);
    native_context()->set_promise_catch_finally_shared_fun(*info);
  }

  {
    Handle<Code> code = handle(
        isolate()->builtins()->builtin(Builtins::kPromiseValueThunkFinally),
        isolate());
    Handle<SharedFunctionInfo> info = factory()->NewSharedFunctionInfo(
        factory()->empty_string(), code, false);
    info->set_internal_formal_parameter_count(0);
    info->set_length(0);
    native_context()->set_promise_value_thunk_finally_shared_fun(*info);
  }

  {
    Handle<Code> code =
        handle(isolate()->builtins()->builtin(Builtins::kPromiseThrowerFinally),
               isolate());
    Handle<SharedFunctionInfo> info = factory()->NewSharedFunctionInfo(
        factory()->empty_string(), code, false);
    info->set_internal_formal_parameter_count(0);
    info->set_length(0);
    native_context()->set_promise_thrower_finally_shared_fun(*info);
  }
}

void Genesis::InitializeGlobal_harmony_regexp_dotall() {
  if (!FLAG_harmony_regexp_dotall) return;

  Handle<JSFunction> constructor(native_context()->regexp_function());
  Handle<JSObject> prototype(JSObject::cast(constructor->instance_prototype()));

  SimpleInstallGetter(prototype, isolate()->factory()->dotAll_string(),
                      Builtins::kRegExpPrototypeDotAllGetter, true);

  // The regexp prototype map has changed because we added a property
  // to it, so we update the saved map.
  Handle<Map> prototype_map(prototype->map());
  Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate());
  native_context()->set_regexp_prototype_map(*prototype_map);
}

#ifdef V8_INTL_SUPPORT
namespace {

void SetFunction(Handle<JSObject> target, Handle<JSFunction> function,
                 Handle<Name> name, PropertyAttributes attributes = DONT_ENUM) {
  JSObject::SetOwnPropertyIgnoreAttributes(target, name, function, attributes)
      .ToHandleChecked();
}

}  // namespace

void Genesis::InitializeGlobal_icu_case_mapping() {
  if (!FLAG_icu_case_mapping) return;

  Handle<JSReceiver> exports_container(
      JSReceiver::cast(native_context()->exports_container()));

  Handle<JSObject> string_prototype(
      JSObject::cast(native_context()->string_function()->prototype()));

  {
    Handle<String> name = factory()->InternalizeUtf8String("toLowerCase");
    SetFunction(string_prototype,
                SimpleCreateFunction(isolate(), name,
                                     Builtins::kStringPrototypeToLowerCaseIntl,
                                     0, true),
                name);
  }
  {
    Handle<String> name = factory()->InternalizeUtf8String("toUpperCase");
    SetFunction(string_prototype,
                SimpleCreateFunction(isolate(), name,
                                     Builtins::kStringPrototypeToUpperCaseIntl,
                                     0, false),
                name);
  }

  Handle<JSFunction> to_locale_lower_case = Handle<JSFunction>::cast(
      JSReceiver::GetProperty(
          exports_container,
          factory()->InternalizeUtf8String("ToLocaleLowerCaseIntl"))
          .ToHandleChecked());
  SetFunction(string_prototype, to_locale_lower_case,
              factory()->InternalizeUtf8String("toLocaleLowerCase"));

  Handle<JSFunction> to_locale_upper_case = Handle<JSFunction>::cast(
      JSReceiver::GetProperty(
          exports_container,
          factory()->InternalizeUtf8String("ToLocaleUpperCaseIntl"))
          .ToHandleChecked());
  SetFunction(string_prototype, to_locale_upper_case,
              factory()->InternalizeUtf8String("toLocaleUpperCase"));
}
#endif

Handle<JSFunction> Genesis::InstallArrayBuffer(Handle<JSObject> target,
                                               const char* name,
                                               Builtins::Name call_byteLength,
                                               BuiltinFunctionId byteLength_id,
                                               Builtins::Name call_slice) {
  // Create the %ArrayBufferPrototype%
  // Setup the {prototype} with the given {name} for @@toStringTag.
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::AddProperty(prototype, factory()->to_string_tag_symbol(),
                        factory()->NewStringFromAsciiChecked(name),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // Allocate the constructor with the given {prototype}.
  Handle<JSFunction> array_buffer_fun =
      InstallFunction(target, name, JS_ARRAY_BUFFER_TYPE,
                      JSArrayBuffer::kSizeWithEmbedderFields, prototype,
                      Builtins::kArrayBufferConstructor);
  array_buffer_fun->shared()->SetConstructStub(
      *isolate()->builtins()->ArrayBufferConstructor_ConstructStub());
  array_buffer_fun->shared()->DontAdaptArguments();
  array_buffer_fun->shared()->set_length(1);

  // Install the "constructor" property on the {prototype}.
  JSObject::AddProperty(prototype, factory()->constructor_string(),
                        array_buffer_fun, DONT_ENUM);

  SimpleInstallFunction(array_buffer_fun, factory()->isView_string(),
                        Builtins::kArrayBufferIsView, 1, true);

  // Install the "byteLength" getter on the {prototype}.
  SimpleInstallGetter(prototype, factory()->byte_length_string(),
                      call_byteLength, false, byteLength_id);

  SimpleInstallFunction(prototype, "slice", call_slice, 2, true);

  return array_buffer_fun;
}


Handle<JSFunction> Genesis::InstallInternalArray(Handle<JSObject> target,
                                                 const char* name,
                                                 ElementsKind elements_kind) {
  // --- I n t e r n a l   A r r a y ---
  // An array constructor on the builtins object that works like
  // the public Array constructor, except that its prototype
  // doesn't inherit from Object.prototype.
  // To be used only for internal work by builtins. Instances
  // must not be leaked to user code.
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSFunction> array_function =
      InstallFunction(target, name, JS_ARRAY_TYPE, JSArray::kSize, prototype,
                      Builtins::kInternalArrayCode);

  InternalArrayConstructorStub internal_array_constructor_stub(isolate());
  Handle<Code> code = internal_array_constructor_stub.GetCode();
  array_function->shared()->SetConstructStub(*code);
  array_function->shared()->DontAdaptArguments();

  Handle<Map> original_map(array_function->initial_map());
  Handle<Map> initial_map = Map::Copy(original_map, "InternalArray");
  initial_map->set_elements_kind(elements_kind);
  JSFunction::SetInitialMap(array_function, initial_map, prototype);

  // Make "length" magic on instances.
  Map::EnsureDescriptorSlack(initial_map, 1);

  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE);

  Handle<AccessorInfo> array_length =
      Accessors::ArrayLengthInfo(isolate(), attribs);
  {  // Add length.
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(array_length->name())), array_length, attribs);
    initial_map->AppendDescriptor(&d);
  }

  return array_function;
}

bool Genesis::InstallNatives(GlobalContextType context_type) {
  HandleScope scope(isolate());

  // Set up the utils object as shared container between native scripts.
  Handle<JSObject> utils = factory()->NewJSObject(isolate()->object_function());
  JSObject::NormalizeProperties(utils, CLEAR_INOBJECT_PROPERTIES, 16,
                                "utils container for native scripts");
  native_context()->set_natives_utils_object(*utils);

  // Set up the extras utils object as a shared container between native
  // scripts and extras. (Extras consume things added there by native scripts.)
  Handle<JSObject> extras_utils =
      factory()->NewJSObject(isolate()->object_function());
  native_context()->set_extras_utils_object(*extras_utils);

  InstallInternalArray(extras_utils, "InternalPackedArray", FAST_ELEMENTS);

  InstallFunction(extras_utils, isolate()->promise_internal_constructor(),
                  factory()->NewStringFromAsciiChecked("createPromise"));
  InstallFunction(extras_utils, isolate()->promise_resolve(),
                  factory()->NewStringFromAsciiChecked("resolvePromise"));
  InstallFunction(extras_utils, isolate()->is_promise(),
                  factory()->NewStringFromAsciiChecked("isPromise"));

  int builtin_index = Natives::GetDebuggerCount();
  // Only run prologue.js at this point.
  DCHECK_EQ(builtin_index, Natives::GetIndex("prologue"));
  if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;

  {
    // Builtin function for OpaqueReference -- a JSValue-based object,
    // that keeps its field isolated from JavaScript code. It may store
    // objects, that JavaScript code may not access.
    Handle<JSFunction> opaque_reference_fun = factory()->NewFunction(
        factory()->empty_string(), isolate()->builtins()->Illegal(),
        isolate()->initial_object_prototype(), JS_VALUE_TYPE, JSValue::kSize);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    JSFunction::SetPrototype(opaque_reference_fun, prototype);
    native_context()->set_opaque_reference_function(*opaque_reference_fun);
  }

  // InternalArrays should not use Smi-Only array optimizations. There are too
  // many places in the C++ runtime code (e.g. RegEx) that assume that
  // elements in InternalArrays can be set to non-Smi values without going
  // through a common bottleneck that would make the SMI_ONLY -> FAST_ELEMENT
  // transition easy to trap. Moreover, they rarely are smi-only.
  {
    HandleScope scope(isolate());
    Handle<JSObject> utils =
        Handle<JSObject>::cast(isolate()->natives_utils_object());
    Handle<JSFunction> array_function =
        InstallInternalArray(utils, "InternalArray", FAST_HOLEY_ELEMENTS);
    native_context()->set_internal_array_function(*array_function);
    InstallInternalArray(utils, "InternalPackedArray", FAST_ELEMENTS);
  }

  // Run the rest of the native scripts.
  while (builtin_index < Natives::GetBuiltinsCount()) {
    if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;
  }

  if (!CallUtilsFunction(isolate(), "PostNatives")) return false;
  auto fast_template_instantiations_cache = isolate()->factory()->NewFixedArray(
      TemplateInfo::kFastTemplateInstantiationsCacheSize);
  native_context()->set_fast_template_instantiations_cache(
      *fast_template_instantiations_cache);

  auto slow_template_instantiations_cache = UnseededNumberDictionary::New(
      isolate(), ApiNatives::kInitialFunctionCacheSize);
  native_context()->set_slow_template_instantiations_cache(
      *slow_template_instantiations_cache);

  // Store the map for the %ObjectPrototype% after the natives has been compiled
  // and the Object function has been set up.
  {
    Handle<JSFunction> object_function(native_context()->object_function());
    DCHECK(JSObject::cast(object_function->initial_map()->prototype())
               ->HasFastProperties());
    native_context()->set_object_function_prototype_map(
        HeapObject::cast(object_function->initial_map()->prototype())->map());
  }

  // Store the map for the %StringPrototype% after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function());
  JSObject* string_function_prototype =
      JSObject::cast(string_function->initial_map()->prototype());
  DCHECK(string_function_prototype->HasFastProperties());
  native_context()->set_string_function_prototype_map(
      string_function_prototype->map());

  Handle<JSGlobalObject> global_object =
      handle(native_context()->global_object());

  // Install Global.decodeURI.
  SimpleInstallFunction(global_object, "decodeURI", Builtins::kGlobalDecodeURI,
                        1, false, kGlobalDecodeURI);

  // Install Global.decodeURIComponent.
  SimpleInstallFunction(global_object, "decodeURIComponent",
                        Builtins::kGlobalDecodeURIComponent, 1, false,
                        kGlobalDecodeURIComponent);

  // Install Global.encodeURI.
  SimpleInstallFunction(global_object, "encodeURI", Builtins::kGlobalEncodeURI,
                        1, false, kGlobalEncodeURI);

  // Install Global.encodeURIComponent.
  SimpleInstallFunction(global_object, "encodeURIComponent",
                        Builtins::kGlobalEncodeURIComponent, 1, false,
                        kGlobalEncodeURIComponent);

  // Install Global.escape.
  SimpleInstallFunction(global_object, "escape", Builtins::kGlobalEscape, 1,
                        false, kGlobalEscape);

  // Install Global.unescape.
  SimpleInstallFunction(global_object, "unescape", Builtins::kGlobalUnescape, 1,
                        false, kGlobalUnescape);

  // Install Global.eval.
  {
    Handle<JSFunction> eval =
        SimpleInstallFunction(global_object, factory()->eval_string(),
                              Builtins::kGlobalEval, 1, false);
    native_context()->set_global_eval_fun(*eval);
  }

  // Install Global.isFinite
  SimpleInstallFunction(global_object, "isFinite", Builtins::kGlobalIsFinite, 1,
                        true, kGlobalIsFinite);

  // Install Global.isNaN
  SimpleInstallFunction(global_object, "isNaN", Builtins::kGlobalIsNaN, 1, true,
                        kGlobalIsNaN);

  // Install Array builtin functions.
  {
    Handle<JSFunction> array_constructor(native_context()->array_function());
    Handle<JSArray> proto(JSArray::cast(array_constructor->prototype()));

    // Verification of important array prototype properties.
    Object* length = proto->length();
    CHECK(length->IsSmi());
    CHECK(Smi::cast(length)->value() == 0);
    CHECK(proto->HasFastSmiOrObjectElements());
    // This is necessary to enable fast checks for absence of elements
    // on Array.prototype and below.
    proto->set_elements(heap()->empty_fixed_array());

    // Install Array.prototype.concat
    Handle<JSFunction> concat =
        InstallFunction(proto, "concat", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        MaybeHandle<JSObject>(), Builtins::kArrayConcat);

    // Make sure that Array.prototype.concat appears to be compiled.
    // The code will never be called, but inline caching for call will
    // only work if it appears to be compiled.
    concat->shared()->DontAdaptArguments();
    DCHECK(concat->is_compiled());
    // Set the lengths for the functions to satisfy ECMA-262.
    concat->shared()->set_length(1);

    // Install Array.prototype.forEach
    Handle<JSFunction> forEach =
        InstallArrayBuiltinFunction(proto, "forEach", Builtins::kArrayForEach);
    // Add forEach to the context.
    native_context()->set_array_for_each_iterator(*forEach);

    // Install Array.prototype.filter
    InstallArrayBuiltinFunction(proto, "filter", Builtins::kArrayFilter);

    // Install Array.prototype.map
    InstallArrayBuiltinFunction(proto, "map", Builtins::kArrayMap);

    // Install Array.prototype.every
    InstallArrayBuiltinFunction(proto, "every", Builtins::kArrayEvery);

    // Install Array.prototype.some
    InstallArrayBuiltinFunction(proto, "some", Builtins::kArraySome);

    // Install Array.prototype.reduce
    InstallArrayBuiltinFunction(proto, "reduce", Builtins::kArrayReduce);

    // Install Array.prototype.reduceRight
    InstallArrayBuiltinFunction(proto, "reduceRight",
                                Builtins::kArrayReduceRight);
  }

  // Install InternalArray.prototype.concat
  {
    Handle<JSFunction> array_constructor(
        native_context()->internal_array_function());
    Handle<JSObject> proto(JSObject::cast(array_constructor->prototype()));
    Handle<JSFunction> concat =
        InstallFunction(proto, "concat", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        MaybeHandle<JSObject>(), Builtins::kArrayConcat);

    // Make sure that InternalArray.prototype.concat appears to be compiled.
    // The code will never be called, but inline caching for call will
    // only work if it appears to be compiled.
    concat->shared()->DontAdaptArguments();
    DCHECK(concat->is_compiled());
    // Set the lengths for the functions to satisfy ECMA-262.
    concat->shared()->set_length(1);
  }

  InstallBuiltinFunctionIds();

  // Create a map for accessor property descriptors (a variant of JSObject
  // that predefines four properties get, set, configurable and enumerable).
  {
    // AccessorPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSAccessorPropertyDescriptor::kSize);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(map, 4);

    {  // get
      Descriptor d = Descriptor::DataField(
          factory()->get_string(), JSAccessorPropertyDescriptor::kGetIndex,
          NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // set
      Descriptor d = Descriptor::DataField(
          factory()->set_string(), JSAccessorPropertyDescriptor::kSetIndex,
          NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(factory()->enumerable_string(),
                                JSAccessorPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      Descriptor d = Descriptor::DataField(
          factory()->configurable_string(),
          JSAccessorPropertyDescriptor::kConfigurableIndex, NONE,
          Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    Map::SetPrototype(map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());
    map->SetInObjectProperties(4);
    map->set_unused_property_fields(0);

    native_context()->set_accessor_property_descriptor_map(*map);
  }

  // Create a map for data property descriptors (a variant of JSObject
  // that predefines four properties value, writable, configurable and
  // enumerable).
  {
    // DataPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSDataPropertyDescriptor::kSize);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(map, 4);

    {  // value
      Descriptor d = Descriptor::DataField(
          factory()->value_string(), JSDataPropertyDescriptor::kValueIndex,
          NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // writable
      Descriptor d =
          Descriptor::DataField(factory()->writable_string(),
                                JSDataPropertyDescriptor::kWritableIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(factory()->enumerable_string(),
                                JSDataPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      Descriptor d =
          Descriptor::DataField(factory()->configurable_string(),
                                JSDataPropertyDescriptor::kConfigurableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    Map::SetPrototype(map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());
    map->SetInObjectProperties(4);
    map->set_unused_property_fields(0);

    native_context()->set_data_property_descriptor_map(*map);
  }

  // Create a constructor for RegExp results (a variant of Array that
  // predefines the two properties index and match).
  {
    // RegExpResult initial map.

    // Find global.Array.prototype to inherit from.
    Handle<JSFunction> array_constructor(native_context()->array_function());
    Handle<JSObject> array_prototype(
        JSObject::cast(array_constructor->instance_prototype()));

    // Add initial map.
    Handle<Map> initial_map =
        factory()->NewMap(JS_ARRAY_TYPE, JSRegExpResult::kSize);
    initial_map->SetConstructor(*array_constructor);

    // Set prototype on map.
    initial_map->set_non_instance_prototype(false);
    Map::SetPrototype(initial_map, array_prototype);

    // Update map with length accessor from Array and add "index" and "input".
    Map::EnsureDescriptorSlack(initial_map, 3);

    {
      JSFunction* array_function = native_context()->array_function();
      Handle<DescriptorArray> array_descriptors(
          array_function->initial_map()->instance_descriptors());
      Handle<String> length = factory()->length_string();
      int old = array_descriptors->SearchWithCache(
          isolate(), *length, array_function->initial_map());
      DCHECK(old != DescriptorArray::kNotFound);
      Descriptor d = Descriptor::AccessorConstant(
          length, handle(array_descriptors->GetValue(old), isolate()),
          array_descriptors->GetDetails(old).attributes());
      initial_map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::DataField(factory()->index_string(),
                                           JSRegExpResult::kIndexIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(&d);
    }

    {
      Descriptor d = Descriptor::DataField(factory()->input_string(),
                                           JSRegExpResult::kInputIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(&d);
    }

    initial_map->SetInObjectProperties(2);
    initial_map->set_unused_property_fields(0);

    native_context()->set_regexp_result_map(*initial_map);
  }

  // Add @@iterator method to the arguments object maps.
  {
    PropertyAttributes attribs = DONT_ENUM;
    Handle<AccessorInfo> arguments_iterator =
        Accessors::ArgumentsIteratorInfo(isolate(), attribs);
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->sloppy_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->fast_aliased_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->slow_aliased_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->strict_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
  }

  return true;
}

bool Genesis::InstallExtraNatives() {
  HandleScope scope(isolate());

  Handle<JSObject> extras_binding =
      factory()->NewJSObject(isolate()->object_function());
  native_context()->set_extras_binding_object(*extras_binding);

  for (int i = ExtraNatives::GetDebuggerCount();
       i < ExtraNatives::GetBuiltinsCount(); i++) {
    if (!Bootstrapper::CompileExtraBuiltin(isolate(), i)) return false;
  }

  return true;
}


bool Genesis::InstallExperimentalExtraNatives() {
  for (int i = ExperimentalExtraNatives::GetDebuggerCount();
       i < ExperimentalExtraNatives::GetBuiltinsCount(); i++) {
    if (!Bootstrapper::CompileExperimentalExtraBuiltin(isolate(), i))
      return false;
  }

  return true;
}


bool Genesis::InstallDebuggerNatives() {
  for (int i = 0; i < Natives::GetDebuggerCount(); ++i) {
    if (!Bootstrapper::CompileBuiltin(isolate(), i)) return false;
  }
  return true;
}


static void InstallBuiltinFunctionId(Handle<JSObject> holder,
                                     const char* function_name,
                                     BuiltinFunctionId id) {
  Isolate* isolate = holder->GetIsolate();
  Handle<Object> function_object =
      JSReceiver::GetProperty(isolate, holder, function_name).ToHandleChecked();
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  function->shared()->set_builtin_function_id(id);
}


#define INSTALL_BUILTIN_ID(holder_expr, fun_name, name) \
  { #holder_expr, #fun_name, k##name }                  \
  ,


void Genesis::InstallBuiltinFunctionIds() {
  HandleScope scope(isolate());
  struct BuiltinFunctionIds {
    const char* holder_expr;
    const char* fun_name;
    BuiltinFunctionId id;
  };

  const BuiltinFunctionIds builtins[] = {
      FUNCTIONS_WITH_ID_LIST(INSTALL_BUILTIN_ID)};

  for (const BuiltinFunctionIds& builtin : builtins) {
    Handle<JSObject> holder =
        ResolveBuiltinIdHolder(native_context(), builtin.holder_expr);
    InstallBuiltinFunctionId(holder, builtin.fun_name, builtin.id);
  }
}

#undef INSTALL_BUILTIN_ID


void Genesis::InitializeNormalizedMapCaches() {
  Handle<NormalizedMapCache> cache = NormalizedMapCache::New(isolate());
  native_context()->set_normalized_map_cache(*cache);
}


bool Bootstrapper::InstallExtensions(Handle<Context> native_context,
                                     v8::ExtensionConfiguration* extensions) {
  BootstrapperActive active(this);
  SaveContext saved_context(isolate_);
  isolate_->set_context(*native_context);
  return Genesis::InstallExtensions(native_context, extensions) &&
      Genesis::InstallSpecialObjects(native_context);
}


bool Genesis::InstallSpecialObjects(Handle<Context> native_context) {
  Isolate* isolate = native_context->GetIsolate();
  // Don't install extensions into the snapshot.
  if (isolate->serializer_enabled()) return true;

  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<JSObject> Error = isolate->error_function();
  Handle<String> name =
      factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("stackTraceLimit"));
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(Error, name, stack_trace_limit, NONE);

  if (FLAG_expose_wasm || FLAG_validate_asm) {
    WasmJs::Install(isolate);
  }

  InstallFFIMap(isolate);

  return true;
}


static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}

Genesis::ExtensionStates::ExtensionStates() : map_(8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  base::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension));
  if (entry == NULL) {
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


bool Genesis::InstallExtensions(Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions) {
  Isolate* isolate = native_context->GetIsolate();
  ExtensionStates extension_states;  // All extensions have state UNVISITED.
  return InstallAutoExtensions(isolate, &extension_states) &&
         (!FLAG_expose_free_buffer ||
          InstallExtension(isolate, "v8/free-buffer", &extension_states)) &&
         (!FLAG_expose_gc ||
          InstallExtension(isolate, "v8/gc", &extension_states)) &&
         (!FLAG_expose_externalize_string ||
          InstallExtension(isolate, "v8/externalize", &extension_states)) &&
         (!FLAG_gc_stats ||
          InstallExtension(isolate, "v8/statistics", &extension_states)) &&
         (!FLAG_expose_trigger_failure ||
          InstallExtension(isolate, "v8/trigger-failure", &extension_states)) &&
         (!FLAG_trace_ignition_dispatches ||
          InstallExtension(isolate, "v8/ignition-statistics",
                           &extension_states)) &&
         InstallRequestedExtensions(isolate, extensions, &extension_states);
}


bool Genesis::InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != NULL;
       it = it->next()) {
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
bool Genesis::InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != NULL;
       it = it->next()) {
    if (strcmp(name, it->extension()->name()) == 0) {
      return InstallExtension(isolate, it, extension_states);
    }
  }
  return Utils::ApiCheck(false,
                         "v8::Context::New()",
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
                       "v8::Context::New()",
                       "Circular extension dependency")) {
    return false;
  }
  DCHECK(extension_states->get_state(current) == UNVISITED);
  extension_states->set_state(current, VISITED);
  v8::Extension* extension = current->extension();
  // Install the extension's dependencies
  for (int i = 0; i < extension->dependency_count(); i++) {
    if (!InstallExtension(isolate,
                          extension->dependencies()[i],
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
  Handle<JSObject> global_proxy(
      JSObject::cast(native_context()->global_proxy()));
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()));

  if (!global_proxy_template.IsEmpty()) {
    // Configure the global proxy object.
    Handle<ObjectTemplateInfo> global_proxy_data =
        v8::Utils::OpenHandle(*global_proxy_template);
    if (!ConfigureApiObject(global_proxy, global_proxy_data)) return false;

    // Configure the global object.
    Handle<FunctionTemplateInfo> proxy_constructor(
        FunctionTemplateInfo::cast(global_proxy_data->constructor()));
    if (!proxy_constructor->prototype_template()->IsUndefined(isolate())) {
      Handle<ObjectTemplateInfo> global_object_data(
          ObjectTemplateInfo::cast(proxy_constructor->prototype_template()));
      if (!ConfigureApiObject(global_object, global_object_data)) return false;
    }
  }

  JSObject::ForceSetPrototype(global_proxy, global_object);

  native_context()->set_array_buffer_map(
      native_context()->array_buffer_fun()->initial_map());
  native_context()->set_js_map_map(
      native_context()->js_map_fun()->initial_map());
  native_context()->set_js_set_map(
      native_context()->js_set_fun()->initial_map());

  return true;
}


bool Genesis::ConfigureApiObject(Handle<JSObject> object,
                                 Handle<ObjectTemplateInfo> object_template) {
  DCHECK(!object_template.is_null());
  DCHECK(FunctionTemplateInfo::cast(object_template->constructor())
             ->IsTemplateFor(object->map()));;

  MaybeHandle<JSObject> maybe_obj =
      ApiNatives::InstantiateObject(object_template);
  Handle<JSObject> obj;
  if (!maybe_obj.ToHandle(&obj)) {
    DCHECK(isolate()->has_pending_exception());
    isolate()->clear_pending_exception();
    return false;
  }
  TransferObject(obj, object);
  return true;
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
        Handle<DescriptorArray>(from->map()->instance_descriptors());
    for (int i = 0; i < from->map()->NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        if (details.kind() == kData) {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          DCHECK(!descs->GetDetails(i).representation().IsDouble());
          Handle<Object> value(from->RawFastPropertyAt(index), isolate());
          JSObject::AddProperty(to, key, value, details.attributes());
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          UNREACHABLE();
        }

      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          DCHECK(!FLAG_track_constant_fields);
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          Handle<Object> value(descs->GetValue(i), isolate());
          JSObject::AddProperty(to, key, value, details.attributes());

        } else {
          DCHECK_EQ(kAccessor, details.kind());
          Handle<Name> key(descs->GetKey(i));
          LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
          CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
          // If the property is already there we skip it
          if (it.IsFound()) continue;
          HandleScope inner(isolate());
          DCHECK(!to->HasFastProperties());
          // Add to dictionary.
          Handle<Object> value(descs->GetValue(i), isolate());
          PropertyDetails d(kAccessor, details.attributes(), i + 1,
                            PropertyCellType::kMutable);
          JSObject::SetNormalizedProperty(to, key, value, d);
        }
      }
    }
  } else if (from->IsJSGlobalObject()) {
    // Copy all keys and values in enumeration order.
    Handle<GlobalDictionary> properties =
        Handle<GlobalDictionary>(from->global_dictionary());
    Handle<FixedArray> key_indices =
        GlobalDictionary::IterationIndices(properties);
    for (int i = 0; i < key_indices->length(); i++) {
      int key_index = Smi::cast(key_indices->get(i))->value();
      Object* raw_key = properties->KeyAt(key_index);
      DCHECK(properties->IsKey(isolate(), raw_key));
      DCHECK(raw_key->IsName());
      // If the property is already there we skip it.
      Handle<Name> key(Name::cast(raw_key), isolate());
      LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
      CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
      if (it.IsFound()) continue;
      // Set the property.
      DCHECK(properties->ValueAt(key_index)->IsPropertyCell());
      Handle<PropertyCell> cell(
          PropertyCell::cast(properties->ValueAt(key_index)), isolate());
      Handle<Object> value(cell->value(), isolate());
      if (value->IsTheHole(isolate())) continue;
      PropertyDetails details = cell->property_details();
      if (details.kind() != kData) continue;
      JSObject::AddProperty(to, key, value, details.attributes());
    }
  } else {
    // Copy all keys and values in enumeration order.
    Handle<NameDictionary> properties =
        Handle<NameDictionary>(from->property_dictionary());
    Handle<FixedArray> key_indices =
        NameDictionary::IterationIndices(properties);
    for (int i = 0; i < key_indices->length(); i++) {
      int key_index = Smi::cast(key_indices->get(i))->value();
      Object* raw_key = properties->KeyAt(key_index);
      DCHECK(properties->IsKey(isolate(), raw_key));
      DCHECK(raw_key->IsName());
      // If the property is already there we skip it.
      Handle<Name> key(Name::cast(raw_key), isolate());
      LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
      CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
      if (it.IsFound()) continue;
      // Set the property.
      Handle<Object> value =
          Handle<Object>(properties->ValueAt(key_index), isolate());
      DCHECK(!value->IsCell());
      DCHECK(!value->IsTheHole(isolate()));
      PropertyDetails details = properties->DetailsAt(key_index);
      DCHECK_EQ(kData, details.kind());
      JSObject::AddProperty(to, key, value, details.attributes());
    }
  }
}


void Genesis::TransferIndexedProperties(Handle<JSObject> from,
                                        Handle<JSObject> to) {
  // Cloning the elements array is sufficient.
  Handle<FixedArray> from_elements =
      Handle<FixedArray>(FixedArray::cast(from->elements()));
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
  Handle<Object> proto(from->map()->prototype(), isolate());
  JSObject::ForceSetPrototype(to, proto);
}


void Genesis::MakeFunctionInstancePrototypeWritable() {
  // The maps with writable prototype are created in CreateEmptyFunction
  // and CreateStrictModeFunctionMaps respectively. Initially the maps are
  // created with read-only prototype for JS builtins processing.
  DCHECK(!sloppy_function_map_writable_prototype_.is_null());
  DCHECK(!strict_function_map_writable_prototype_.is_null());

  // Replace function instance maps to make prototype writable.
  native_context()->set_sloppy_function_map(
      *sloppy_function_map_writable_prototype_);
  native_context()->set_strict_function_map(
      *strict_function_map_writable_prototype_);
}


Genesis::Genesis(
    Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    GlobalContextType context_type)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    return;
  }

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
      Object* size = isolate->heap()->serialized_global_proxy_sizes()->get(
          static_cast<int>(context_snapshot_index) - 1);
      instance_size = Smi::cast(size)->value();
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
  if (!isolate->initialized_from_snapshot() ||
      !Snapshot::NewContextFromSnapshot(isolate, global_proxy,
                                        context_snapshot_index,
                                        embedder_fields_deserializer)
           .ToHandle(&native_context_)) {
    native_context_ = Handle<Context>();
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(*native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();
#if V8_TRACE_MAPS
    if (FLAG_trace_maps) {
      Handle<JSFunction> object_fun = isolate->object_function();
      PrintF("[TraceMap: InitialMap map= %p SFI= %d_Object ]\n",
             reinterpret_cast<void*>(object_fun->initial_map()),
             object_fun->shared()->unique_id());
      Map::TraceAllTransitions(object_fun->initial_map());
    }
#endif

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
    DCHECK_EQ(0u, context_snapshot_index);
    // We get here if there was no context snapshot.
    CreateRoots();
    Handle<JSFunction> empty_function = CreateEmptyFunction(isolate);
    CreateStrictModeFunctionMaps(empty_function);
    CreateIteratorMaps(empty_function);
    CreateAsyncIteratorMaps(empty_function);
    CreateAsyncFunctionMaps(empty_function);
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);
    InitializeGlobal(global_object, empty_function, context_type);
    InitializeNormalizedMapCaches();

    if (!InstallNatives(context_type)) return;

    MakeFunctionInstancePrototypeWritable();

    if (!InstallExtraNatives()) return;
    if (!ConfigureGlobalObjects(global_proxy_template)) return;

    isolate->counters()->contexts_created_from_scratch()->Increment();
  }

  // Install experimental natives. Do not include them into the
  // snapshot as we should be able to turn them off at runtime. Re-installing
  // them after they have already been deserialized would also fail.
  if (context_type == FULL_CONTEXT) {
    if (!isolate->serializer_enabled()) {
      InitializeExperimentalGlobal();

      if (FLAG_experimental_extras) {
        if (!InstallExperimentalExtraNatives()) return;
      }

      // Store String.prototype's map again in case it has been changed by
      // experimental natives.
      Handle<JSFunction> string_function(native_context()->string_function());
      JSObject* string_function_prototype =
          JSObject::cast(string_function->initial_map()->prototype());
      DCHECK(string_function_prototype->HasFastProperties());
      native_context()->set_string_function_prototype_map(
          string_function_prototype->map());
    }
  } else if (context_type == DEBUG_CONTEXT) {
    DCHECK(!isolate->serializer_enabled());
    InitializeExperimentalGlobal();
    if (!InstallDebuggerNatives()) return;
  }

  ConfigureUtilsObject(context_type);

  // Check that the script context table is empty except for the 'this' binding.
  // We do not need script contexts for native scripts.
  DCHECK_EQ(1, native_context()->script_context_table()->used());

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

  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    return;
  }

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
      FunctionTemplateInfo::cast(global_proxy_data->constructor()));

  Handle<ObjectTemplateInfo> global_object_template(
      ObjectTemplateInfo::cast(global_constructor->prototype_template()));
  Handle<JSObject> global_object =
      ApiNatives::InstantiateRemoteObject(
          global_object_template).ToHandleChecked();

  // (Re)initialize the global proxy object.
  DCHECK_EQ(global_proxy_data->embedder_field_count(),
            global_proxy_template->InternalFieldCount());
  Handle<Map> global_proxy_map = isolate->factory()->NewMap(
      JS_GLOBAL_PROXY_TYPE, proxy_size, FAST_HOLEY_SMI_ELEMENTS);
  global_proxy_map->set_is_access_check_needed(true);
  global_proxy_map->set_has_hidden_prototype(true);

  // A remote global proxy has no native context.
  global_proxy->set_native_context(heap()->null_value());

  // Configure the hidden prototype chain of the global proxy.
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->map()->SetConstructor(*global_constructor);
  // TODO(dcheng): This is a hack. Why does this need to be manually called
  // here? Line 4812 should have taken care of it?
  global_proxy->map()->set_has_hidden_prototype(true);

  global_proxy_ = global_proxy;
}

// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() {
  return sizeof(NestingCounterType);
}


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
void Bootstrapper::FreeThreadResources() {
  DCHECK(!IsActive());
}

}  // namespace internal
}  // namespace v8

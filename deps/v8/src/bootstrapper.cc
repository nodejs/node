// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"

#include "src/accessors.h"
#include "src/api-natives.h"
#include "src/code-stubs.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/free-buffer-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-js.h"

namespace v8 {
namespace internal {

Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION) {}

template <class Source>
Handle<String> Bootstrapper::SourceLookup(int index) {
  DCHECK(0 <= index && index < Source::GetBuiltinsCount());
  Heap* heap = isolate_->heap();
  if (Source::GetSourceCache(heap)->get(index)->IsUndefined()) {
    // We can use external strings for the natives.
    Vector<const char> source = Source::GetScriptSource(index);
    NativesExternalStringResource* resource =
        new NativesExternalStringResource(source.start(), source.length());
    // We do not expect this to throw an exception. Change this if it does.
    Handle<String> source_code = isolate_->factory()
                                     ->NewExternalStringFromOneByte(resource)
                                     .ToHandleChecked();
    // Mark this external string with a special map.
    source_code->set_map(isolate_->heap()->native_source_string_map());
    Source::GetSourceCache(heap)->set(index, *source_code);
  }
  Handle<Object> cached_source(Source::GetSourceCache(heap)->get(index),
                               isolate_);
  return Handle<String>::cast(cached_source);
}


template Handle<String> Bootstrapper::SourceLookup<Natives>(int index);
template Handle<String> Bootstrapper::SourceLookup<ExperimentalNatives>(
    int index);
template Handle<String> Bootstrapper::SourceLookup<ExperimentalExtraNatives>(
    int index);
template Handle<String> Bootstrapper::SourceLookup<ExtraNatives>(int index);


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
}


void DeleteNativeSources(Object* maybe_array) {
  if (maybe_array->IsFixedArray()) {
    FixedArray* array = FixedArray::cast(maybe_array);
    for (int i = 0; i < array->length(); i++) {
      Object* natives_source = array->get(i);
      if (!natives_source->IsUndefined()) {
        const NativesExternalStringResource* resource =
            reinterpret_cast<const NativesExternalStringResource*>(
                ExternalOneByteString::cast(natives_source)->resource());
        delete resource;
      }
    }
  }
}


void Bootstrapper::TearDown() {
  DeleteNativeSources(Natives::GetSourceCache(isolate_->heap()));
  DeleteNativeSources(ExperimentalNatives::GetSourceCache(isolate_->heap()));
  DeleteNativeSources(ExtraNatives::GetSourceCache(isolate_->heap()));
  DeleteNativeSources(
      ExperimentalExtraNatives::GetSourceCache(isolate_->heap()));

  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}


class Genesis BASE_EMBEDDED {
 public:
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template,
          v8::ExtensionConfiguration* extensions,
          GlobalContextType context_type);
  ~Genesis() { }

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Heap* heap() const { return isolate_->heap(); }

  Handle<Context> result() { return result_; }

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
  void CreateStrongModeFunctionMaps(Handle<JSFunction> empty);
  void CreateIteratorMaps();

  // Make the "arguments" and "caller" properties throw a TypeError on access.
  void AddRestrictedFunctionProperties(Handle<Map> map);

  // Creates the global objects using the global proxy and the template passed
  // in through the API.  We call this regardless of whether we are building a
  // context from scratch or using a deserialized one from the partial snapshot
  // but in the latter case we don't use the objects it produces directly, as
  // we have to used the deserialized ones that are linked together with the
  // rest of the context snapshot.
  Handle<JSGlobalObject> CreateNewGlobals(
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      Handle<JSGlobalProxy> global_proxy);
  // Hooks the given global proxy into the context.  If the context was created
  // by deserialization then this will unhook the global proxy that was
  // deserialized, leaving the GC to pick it up.
  void HookUpGlobalProxy(Handle<JSGlobalObject> global_object,
                         Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the global that has been created by the templates
  // passed through the API.  The global from the snapshot is detached from the
  // other objects in the snapshot.
  void HookUpGlobalObject(Handle<JSGlobalObject> global_object);
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
  DECLARE_FEATURE_INITIALIZATION(promise_extra, "")
#undef DECLARE_FEATURE_INITIALIZATION

  Handle<JSFunction> InstallArrayBuffer(Handle<JSObject> target,
                                        const char* name);
  Handle<JSFunction> InstallInternalArray(Handle<JSObject> target,
                                          const char* name,
                                          ElementsKind elements_kind);
  bool InstallNatives(GlobalContextType context_type);

  void InstallTypedArray(const char* name, ElementsKind elements_kind,
                         Handle<JSFunction>* fun);
  bool InstallExperimentalNatives();
  bool InstallExtraNatives();
  bool InstallExperimentalExtraNatives();
  bool InstallDebuggerNatives();
  void InstallBuiltinFunctionIds();
  void InstallExperimentalBuiltinFunctionIds();
  void InitializeNormalizedMapCaches();
  void InstallJSProxyMaps();

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
    HashMap map_;
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

  enum FunctionMode {
    // With prototype.
    FUNCTION_WITH_WRITEABLE_PROTOTYPE,
    FUNCTION_WITH_READONLY_PROTOTYPE,
    // Without prototype.
    FUNCTION_WITHOUT_PROTOTYPE
  };

  static bool IsFunctionModeWithPrototype(FunctionMode function_mode) {
    return (function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ||
            function_mode == FUNCTION_WITH_READONLY_PROTOTYPE);
  }

  Handle<Map> CreateSloppyFunctionMap(FunctionMode function_mode);

  void SetFunctionInstanceDescriptor(Handle<Map> map,
                                     FunctionMode function_mode);
  void MakeFunctionInstancePrototypeWritable();

  Handle<Map> CreateStrictFunctionMap(FunctionMode function_mode,
                                      Handle<JSFunction> empty_function);
  Handle<Map> CreateStrongFunctionMap(Handle<JSFunction> empty_function,
                                      bool is_constructor);


  void SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                           FunctionMode function_mode);
  void SetStrongFunctionInstanceDescriptor(Handle<Map> map);

  static bool CallUtilsFunction(Isolate* isolate, const char* name);

  static bool CompileExtension(Isolate* isolate, v8::Extension* extension);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<Context> native_context_;

  // Function maps. Function maps are created initially with a read only
  // prototype for the processing of JS builtins. Later the function maps are
  // replaced in order to make prototype writable. These are the final, writable
  // prototype, maps.
  Handle<Map> sloppy_function_map_writable_prototype_;
  Handle<Map> strict_function_map_writable_prototype_;
  Handle<JSFunction> strict_poison_function_;
  Handle<JSFunction> restricted_function_properties_thrower_;

  BootstrapperActive active_;
  friend class Bootstrapper;
};


void Bootstrapper::Iterate(ObjectVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}

Handle<Context> Bootstrapper::CreateEnvironment(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    v8::ExtensionConfiguration* extensions, GlobalContextType context_type) {
  HandleScope scope(isolate_);
  Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,
                  extensions, context_type);
  Handle<Context> env = genesis.result();
  if (env.is_null() ||
      (context_type != THIN_CONTEXT && !InstallExtensions(env, extensions))) {
    return Handle<Context>();
  }
  return scope.CloseAndEscape(env);
}


static void SetObjectPrototype(Handle<JSObject> object, Handle<Object> proto) {
  // object.__proto__ = proto;
  Handle<Map> old_map = Handle<Map>(object->map());
  Handle<Map> new_map = Map::Copy(old_map, "SetObjectPrototype");
  Map::SetPrototype(new_map, proto, FAST_PROTOTYPE);
  JSObject::MigrateToMap(object, new_map);
}


void Bootstrapper::DetachGlobal(Handle<Context> env) {
  env->GetIsolate()->counters()->errors_thrown_per_context()->AddSample(
    env->GetErrorsThrown());

  Factory* factory = env->GetIsolate()->factory();
  Handle<JSGlobalProxy> global_proxy(JSGlobalProxy::cast(env->global_proxy()));
  global_proxy->set_native_context(*factory->null_value());
  SetObjectPrototype(global_proxy, factory->null_value());
  global_proxy->map()->SetConstructor(*factory->null_value());
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
  function->shared()->set_native(true);
}


static void InstallFunction(Handle<JSObject> target,
                            Handle<JSFunction> function, Handle<Name> name,
                            PropertyAttributes attributes = DONT_ENUM) {
  Handle<String> name_string = Name::ToFunctionName(name).ToHandleChecked();
  InstallFunction(target, name, function, name_string, attributes);
}


static Handle<JSFunction> CreateFunction(Isolate* isolate, Handle<String> name,
                                         InstanceType type, int instance_size,
                                         MaybeHandle<JSObject> maybe_prototype,
                                         Builtins::Name call,
                                         bool strict_function_map = false) {
  Factory* factory = isolate->factory();
  Handle<Code> call_code(isolate->builtins()->builtin(call));
  Handle<JSObject> prototype;
  static const bool kReadOnlyPrototype = false;
  static const bool kInstallConstructor = false;
  return maybe_prototype.ToHandle(&prototype)
             ? factory->NewFunction(name, call_code, prototype, type,
                                    instance_size, kReadOnlyPrototype,
                                    kInstallConstructor, strict_function_map)
             : factory->NewFunctionWithoutPrototype(name, call_code,
                                                    strict_function_map);
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

}  // namespace


void Genesis::SetFunctionInstanceDescriptor(Handle<Map> map,
                                            FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 5 : 4;
  Map::EnsureDescriptorSlack(map, size);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  Handle<AccessorInfo> length =
      Accessors::FunctionLengthInfo(isolate(), roc_attribs);
  {  // Add length.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(length->name())),
                                 length, roc_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> name =
      Accessors::FunctionNameInfo(isolate(), ro_attribs);
  {  // Add name.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(name->name())), name,
                                 roc_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> args =
      Accessors::FunctionArgumentsInfo(isolate(), ro_attribs);
  {  // Add arguments.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(args->name())), args,
                                 ro_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> caller =
      Accessors::FunctionCallerInfo(isolate(), ro_attribs);
  {  // Add caller.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(caller->name())),
                                 caller, ro_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    if (function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE) {
      ro_attribs = static_cast<PropertyAttributes>(ro_attribs & ~READ_ONLY);
    }
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), ro_attribs);
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(prototype->name())),
                                 prototype, ro_attribs);
    map->AppendDescriptor(&d);
  }
}


Handle<Map> Genesis::CreateSloppyFunctionMap(FunctionMode function_mode) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetFunctionInstanceDescriptor(map, function_mode);
  map->set_is_constructor(IsFunctionModeWithPrototype(function_mode));
  map->set_is_callable();
  return map;
}


Handle<JSFunction> Genesis::CreateEmptyFunction(Isolate* isolate) {
  // Allocate the map for function instances. Maps are allocated first and their
  // prototypes patched later, once empty function is created.

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  native_context()->set_sloppy_function_without_prototype_map(
      *function_without_prototype_map);

  // Allocate the function map. This map is temporary, used only for processing
  // of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> function_map =
      CreateSloppyFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE);
  native_context()->set_sloppy_function_map(*function_map);
  native_context()->set_sloppy_function_with_readonly_prototype_map(
      *function_map);

  // The final map for functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  sloppy_function_map_writable_prototype_ =
      CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE);
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
    object_function_prototype->set_map(*map);

    native_context()->set_initial_object_prototype(*object_function_prototype);
    // For bootstrapping set the array prototype to be the same as the object
    // prototype, otherwise the missing initial_array_prototype will cause
    // assertions during startup.
    native_context()->set_initial_array_prototype(*object_function_prototype);
    Accessors::FunctionSetPrototype(object_fun, object_function_prototype)
        .Assert();

    // Allocate initial strong object map.
    Handle<Map> strong_object_map =
        Map::Copy(Handle<Map>(object_fun->initial_map()), "EmptyStrongObject");
    strong_object_map->set_is_strong();
    native_context()->set_js_object_strong_map(*strong_object_map);
  }

  // Allocate the empty function as the prototype for function - ES6 19.2.3
  Handle<Code> code(isolate->builtins()->EmptyFunction());
  Handle<JSFunction> empty_function =
      factory->NewFunctionWithoutPrototype(factory->empty_string(), code);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_function_map =
      CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  DCHECK(!empty_function_map->is_dictionary_map());
  Map::SetPrototype(empty_function_map, object_function_prototype);
  empty_function_map->set_is_prototype_map(true);

  empty_function->set_map(*empty_function_map);

  // --- E m p t y ---
  Handle<String> source = factory->NewStringFromStaticChars("() {}");
  Handle<Script> script = factory->NewScript(source);
  script->set_type(Script::TYPE_NATIVE);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
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

  // ES6 draft 03-17-2015, section 8.2.2 step 12
  AddRestrictedFunctionProperties(empty_function_map);

  return empty_function;
}


void Genesis::SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                                  FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 3 : 2;
  Map::EnsureDescriptorSlack(map, size);

  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  DCHECK(function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ||
         function_mode == FUNCTION_WITH_READONLY_PROTOTYPE ||
         function_mode == FUNCTION_WITHOUT_PROTOTYPE);
  {  // Add length.
    Handle<AccessorInfo> length =
        Accessors::FunctionLengthInfo(isolate(), roc_attribs);
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(length->name())),
                                 length, roc_attribs);
    map->AppendDescriptor(&d);
  }
  {  // Add name.
    Handle<AccessorInfo> name =
        Accessors::FunctionNameInfo(isolate(), roc_attribs);
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(name->name())), name,
                                 roc_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype.
    PropertyAttributes attribs =
        function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ? rw_attribs
                                                           : ro_attribs;
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), attribs);
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(prototype->name())),
                                 prototype, attribs);
    map->AppendDescriptor(&d);
  }
}


void Genesis::SetStrongFunctionInstanceDescriptor(Handle<Map> map) {
  Map::EnsureDescriptorSlack(map, 2);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

  Handle<AccessorInfo> length =
      Accessors::FunctionLengthInfo(isolate(), ro_attribs);
  {  // Add length.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(length->name())),
                                 length, ro_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> name =
      Accessors::FunctionNameInfo(isolate(), ro_attribs);
  {  // Add name.
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(name->name())), name,
                                 ro_attribs);
    map->AppendDescriptor(&d);
  }
}


// Creates the %ThrowTypeError% function.
Handle<JSFunction> Genesis::GetThrowTypeErrorIntrinsic(
    Builtins::Name builtin_name) {
  Handle<String> name =
      factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("ThrowTypeError"));
  Handle<Code> code(isolate()->builtins()->builtin(builtin_name));
  Handle<JSFunction> function =
      factory()->NewFunctionWithoutPrototype(name, code);
  function->shared()->DontAdaptArguments();

  // %ThrowTypeError% must not have a name property.
  if (JSReceiver::DeleteProperty(function, factory()->name_string())
          .IsNothing()) {
    DCHECK(false);
  }

  // length needs to be non configurable.
  Handle<Object> value(Smi::FromInt(function->shared()->length()), isolate());
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->length_string(), value,
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  if (JSObject::PreventExtensions(function, Object::THROW_ON_ERROR)
          .IsNothing()) {
    DCHECK(false);
  }

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


Handle<Map> Genesis::CreateStrictFunctionMap(
    FunctionMode function_mode, Handle<JSFunction> empty_function) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetStrictFunctionInstanceDescriptor(map, function_mode);
  map->set_is_constructor(IsFunctionModeWithPrototype(function_mode));
  map->set_is_callable();
  Map::SetPrototype(map, empty_function);
  return map;
}


Handle<Map> Genesis::CreateStrongFunctionMap(
    Handle<JSFunction> empty_function, bool is_constructor) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetStrongFunctionInstanceDescriptor(map);
  map->set_is_constructor(is_constructor);
  Map::SetPrototype(map, empty_function);
  map->set_is_callable();
  map->set_is_extensible(is_constructor);
  map->set_is_strong();
  return map;
}


void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  // Allocate map for the prototype-less strict mode instances.
  Handle<Map> strict_function_without_prototype_map =
      CreateStrictFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_strict_function_without_prototype_map(
      *strict_function_without_prototype_map);

  // Allocate map for the strict mode functions. This map is temporary, used
  // only for processing of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> strict_function_map =
      CreateStrictFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_function_map(*strict_function_map);

  // The final map for the strict mode functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  strict_function_map_writable_prototype_ =
      CreateStrictFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE, empty);
}


void Genesis::CreateStrongModeFunctionMaps(Handle<JSFunction> empty) {
  // Allocate map for strong mode instances, which never have prototypes.
  Handle<Map> strong_function_map = CreateStrongFunctionMap(empty, false);
  native_context()->set_strong_function_map(*strong_function_map);
  // Constructors do, though.
  Handle<Map> strong_constructor_map = CreateStrongFunctionMap(empty, true);
  native_context()->set_strong_constructor_map(*strong_constructor_map);
}


void Genesis::CreateIteratorMaps() {
  // Create iterator-related meta-objects.
  Handle<JSObject> iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSObject> generator_object_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSObject> generator_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  SetObjectPrototype(generator_object_prototype, iterator_prototype);

  JSObject::AddProperty(generator_function_prototype,
                        factory()->InternalizeUtf8String("prototype"),
                        generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  Handle<Map> strict_function_map(strict_function_map_writable_prototype_);
  // Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> sloppy_generator_function_map =
      Map::Copy(strict_function_map, "SloppyGeneratorFunction");
  sloppy_generator_function_map->set_is_constructor(false);
  Map::SetPrototype(sloppy_generator_function_map,
                    generator_function_prototype);
  native_context()->set_sloppy_generator_function_map(
      *sloppy_generator_function_map);

  Handle<Map> strict_generator_function_map =
      Map::Copy(strict_function_map, "StrictGeneratorFunction");
  strict_generator_function_map->set_is_constructor(false);
  Map::SetPrototype(strict_generator_function_map,
                    generator_function_prototype);
  native_context()->set_strict_generator_function_map(
      *strict_generator_function_map);

  Handle<Map> strong_function_map(native_context()->strong_function_map());
  Handle<Map> strong_generator_function_map =
      Map::Copy(strong_function_map, "StrongGeneratorFunction");
  strong_generator_function_map->set_is_constructor(false);
  Map::SetPrototype(strong_generator_function_map,
                    generator_function_prototype);
  native_context()->set_strong_generator_function_map(
      *strong_generator_function_map);

  Handle<JSFunction> object_function(native_context()->object_function());
  Handle<Map> generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(generator_object_prototype_map, generator_object_prototype);
  native_context()->set_generator_object_prototype_map(
      *generator_object_prototype_map);
}


static void ReplaceAccessors(Handle<Map> map,
                             Handle<String> name,
                             PropertyAttributes attributes,
                             Handle<AccessorPair> accessor_pair) {
  DescriptorArray* descriptors = map->instance_descriptors();
  int idx = descriptors->SearchWithCache(map->GetIsolate(), *name, *map);
  AccessorConstantDescriptor descriptor(name, accessor_pair, attributes);
  descriptors->Replace(idx, &descriptor);
}


void Genesis::AddRestrictedFunctionProperties(Handle<Map> map) {
  PropertyAttributes rw_attribs = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> thrower = GetRestrictedFunctionPropertiesThrower();
  Handle<AccessorPair> accessors = factory()->NewAccessorPair();
  accessors->set_getter(*thrower);
  accessors->set_setter(*thrower);

  ReplaceAccessors(map, factory()->arguments_string(), rw_attribs, accessors);
  ReplaceAccessors(map, factory()->caller_string(), rw_attribs, accessors);
}


static void AddToWeakNativeContextList(Context* context) {
  DCHECK(context->IsNativeContext());
  Heap* heap = context->GetIsolate()->heap();
#ifdef DEBUG
  { // NOLINT
    DCHECK(context->get(Context::NEXT_CONTEXT_LINK)->IsUndefined());
    // Check that context is not in the list yet.
    for (Object* current = heap->native_contexts_list();
         !current->IsUndefined();
         current = Context::cast(current)->get(Context::NEXT_CONTEXT_LINK)) {
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
    v8::NeanderArray listeners(isolate());
    native_context()->set_message_listeners(*listeners.value());
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
    if (!proto_template->IsUndefined()) {
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
    Handle<Object> value = JSReceiver::GetProperty(&it).ToHandleChecked();
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
    global_proxy_function = factory()->NewFunction(
        name, code, JS_GLOBAL_PROXY_TYPE, JSGlobalProxy::kSize);
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

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  return global_object;
}


void Genesis::HookUpGlobalProxy(Handle<JSGlobalObject> global_object,
                                Handle<JSGlobalProxy> global_proxy) {
  // Set the native context for the global object.
  global_object->set_native_context(*native_context());
  global_object->set_global_proxy(*global_proxy);
  global_proxy->set_native_context(*native_context());
  // If we deserialized the context, the global proxy is already
  // correctly set up. Otherwise it's undefined.
  DCHECK(native_context()->get(Context::GLOBAL_PROXY_INDEX)->IsUndefined() ||
         native_context()->global_proxy() == *global_proxy);
  native_context()->set_global_proxy(*global_proxy);
}


void Genesis::HookUpGlobalObject(Handle<JSGlobalObject> global_object) {
  Handle<JSGlobalObject> global_object_from_snapshot(
      JSGlobalObject::cast(native_context()->extension()));
  native_context()->set_extension(*global_object);
  native_context()->set_security_token(*global_object);

  TransferNamedProperties(global_object_from_snapshot, global_object);
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}


static Handle<JSFunction> SimpleCreateFunction(Isolate* isolate,
                                               Handle<String> name,
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


static Handle<JSFunction> SimpleInstallFunction(Handle<JSObject> base,
                                                Handle<String> name,
                                                Builtins::Name call, int len,
                                                bool adapt) {
  Handle<JSFunction> fun =
      SimpleCreateFunction(base->GetIsolate(), name, call, len, adapt);
  InstallFunction(base, fun, name, DONT_ENUM);
  return fun;
}


static Handle<JSFunction> SimpleInstallFunction(Handle<JSObject> base,
                                                const char* name,
                                                Builtins::Name call, int len,
                                                bool adapt) {
  Factory* const factory = base->GetIsolate()->factory();
  return SimpleInstallFunction(base, factory->InternalizeUtf8String(name), call,
                               len, adapt);
}


static void InstallWithIntrinsicDefaultProto(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             int context_index) {
  Handle<Smi> index(Smi::FromInt(context_index), isolate);
  JSObject::AddProperty(
      function, isolate->factory()->native_context_index_symbol(), index, NONE);
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
    SimpleInstallFunction(object_function, factory->create_string(),
                          Builtins::kObjectCreate, 2, false);
    Handle<JSFunction> object_freeze = SimpleInstallFunction(
        object_function, "freeze", Builtins::kObjectFreeze, 1, false);
    native_context()->set_object_freeze(*object_freeze);
    SimpleInstallFunction(object_function, "getOwnPropertyDescriptor",
                          Builtins::kObjectGetOwnPropertyDescriptor, 2, false);
    SimpleInstallFunction(object_function, "getOwnPropertyNames",
                          Builtins::kObjectGetOwnPropertyNames, 1, false);
    SimpleInstallFunction(object_function, "getOwnPropertySymbols",
                          Builtins::kObjectGetOwnPropertySymbols, 1, false);
    SimpleInstallFunction(object_function, "is", Builtins::kObjectIs, 2, true);
    Handle<JSFunction> object_is_extensible =
        SimpleInstallFunction(object_function, "isExtensible",
                              Builtins::kObjectIsExtensible, 1, false);
    native_context()->set_object_is_extensible(*object_is_extensible);
    Handle<JSFunction> object_is_frozen = SimpleInstallFunction(
        object_function, "isFrozen", Builtins::kObjectIsFrozen, 1, false);
    native_context()->set_object_is_frozen(*object_is_frozen);
    Handle<JSFunction> object_is_sealed = SimpleInstallFunction(
        object_function, "isSealed", Builtins::kObjectIsSealed, 1, false);
    native_context()->set_object_is_sealed(*object_is_sealed);
    Handle<JSFunction> object_keys = SimpleInstallFunction(
        object_function, "keys", Builtins::kObjectKeys, 1, false);
    native_context()->set_object_keys(*object_keys);
    SimpleInstallFunction(object_function, "preventExtensions",
                          Builtins::kObjectPreventExtensions, 1, false);
    SimpleInstallFunction(object_function, "seal", Builtins::kObjectSeal, 1,
                          false);
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
    function_fun->shared()->set_construct_stub(
        *isolate->builtins()->FunctionConstructor());
    function_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, function_fun,
                                     Context::FUNCTION_FUNCTION_INDEX);

    // Setup the methods on the %FunctionPrototype%.
    SimpleInstallFunction(prototype, factory->apply_string(),
                          Builtins::kFunctionPrototypeApply, 2, false);
    SimpleInstallFunction(prototype, factory->bind_string(),
                          Builtins::kFunctionPrototypeBind, 1, false);
    SimpleInstallFunction(prototype, factory->call_string(),
                          Builtins::kFunctionPrototypeCall, 1, false);
    SimpleInstallFunction(prototype, factory->toString_string(),
                          Builtins::kFunctionPrototypeToString, 0, false);

    // Install the @@hasInstance function.
    Handle<JSFunction> has_instance = InstallFunction(
        prototype, factory->has_instance_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, MaybeHandle<JSObject>(),
        Builtins::kFunctionHasInstance,
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY));

    // Set the expected parameters for @@hasInstance to 1; required by builtin.
    has_instance->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    has_instance->shared()->set_length(1);

    // Install in the native context
    native_context()->set_ordinary_has_instance(*has_instance);

    // Install the "constructor" property on the %FunctionPrototype%.
    JSObject::AddProperty(prototype, factory->constructor_string(),
                          function_fun, DONT_ENUM);

    sloppy_function_map_writable_prototype_->SetConstructor(*function_fun);
    strict_function_map_writable_prototype_->SetConstructor(*function_fun);
    native_context()->strong_function_map()->SetConstructor(*function_fun);
  }

  {  // --- A r r a y ---
    Handle<JSFunction> array_function =
        InstallFunction(global, "Array", JS_ARRAY_TYPE, JSArray::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kArrayCode);
    array_function->shared()->DontAdaptArguments();
    array_function->shared()->set_function_data(Smi::FromInt(kArrayCode));

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
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(array_length->name())), array_length,
          attribs);
      initial_map->AppendDescriptor(&d);
    }

    InstallWithIntrinsicDefaultProto(isolate, array_function,
                                     Context::ARRAY_FUNCTION_INDEX);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(native_context(), initial_map);
    ArrayConstructorStub array_constructor_stub(isolate);
    Handle<Code> code = array_constructor_stub.GetCode();
    array_function->shared()->set_construct_stub(*code);

    Handle<Map> initial_strong_map =
        Map::Copy(initial_map, "SetInstancePrototype");
    initial_strong_map->set_is_strong();
    CacheInitialJSArrayMaps(native_context(), initial_strong_map);

    Handle<JSFunction> is_arraylike = SimpleInstallFunction(
        array_function, isolate->factory()->InternalizeUtf8String("isArray"),
        Builtins::kArrayIsArray, 1, true);
    native_context()->set_is_arraylike(*is_arraylike);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun = InstallFunction(
        global, "Number", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kNumberConstructor);
    number_fun->shared()->DontAdaptArguments();
    number_fun->shared()->set_construct_stub(
        *isolate->builtins()->NumberConstructor_ConstructStub());
    number_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, number_fun,
                                     Context::NUMBER_FUNCTION_INDEX);
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kBooleanConstructor);
    boolean_fun->shared()->DontAdaptArguments();
    boolean_fun->shared()->set_construct_stub(
        *isolate->builtins()->BooleanConstructor_ConstructStub());
    boolean_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate, boolean_fun,
                                     Context::BOOLEAN_FUNCTION_INDEX);

    // Create the %BooleanPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(boolean_fun, TENURED));
    prototype->set_value(isolate->heap()->false_value());
    Accessors::FunctionSetPrototype(boolean_fun, prototype).Assert();

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), boolean_fun,
                          DONT_ENUM);

    // Install the Boolean.prototype methods.
    SimpleInstallFunction(prototype, "toString",
                          Builtins::kBooleanPrototypeToString, 0, false);
    SimpleInstallFunction(prototype, "valueOf",
                          Builtins::kBooleanPrototypeValueOf, 0, false);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun = InstallFunction(
        global, "String", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kStringConstructor);
    string_fun->shared()->set_construct_stub(
        *isolate->builtins()->StringConstructor_ConstructStub());
    string_fun->shared()->DontAdaptArguments();
    string_fun->shared()->set_length(1);
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
      AccessorConstantDescriptor d(factory->length_string(), string_length,
                                   attribs);
      string_map->AppendDescriptor(&d);
    }

    // Install the String.fromCharCode function.
    SimpleInstallFunction(string_fun, "fromCharCode",
                          Builtins::kStringFromCharCode, 1, false);
  }

  {
    // --- S y m b o l ---
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Handle<JSFunction> symbol_fun =
        InstallFunction(global, "Symbol", JS_VALUE_TYPE, JSValue::kSize,
                        prototype, Builtins::kSymbolConstructor);
    symbol_fun->shared()->set_construct_stub(
        *isolate->builtins()->SymbolConstructor_ConstructStub());
    symbol_fun->shared()->set_length(1);
    symbol_fun->shared()->DontAdaptArguments();
    native_context()->set_symbol_function(*symbol_fun);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(prototype, factory->constructor_string(), symbol_fun,
                          DONT_ENUM);
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
    date_fun->shared()->set_construct_stub(
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
                          0, false);
    SimpleInstallFunction(prototype, "getYear", Builtins::kDatePrototypeGetYear,
                          0, true);
    SimpleInstallFunction(prototype, "setYear", Builtins::kDatePrototypeSetYear,
                          1, false);

    // Install i18n fallback functions.
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

  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, regexp_fun,
                                     Context::REGEXP_FUNCTION_INDEX);
    regexp_fun->shared()->set_construct_stub(
        *isolate->builtins()->JSBuiltinsConstructStub());

    DCHECK(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map());

    DCHECK_EQ(0, initial_map->GetInObjectProperties());

    Map::EnsureDescriptorSlack(initial_map, 1);

    // ECMA-262, section 15.10.7.5.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    DataDescriptor field(factory->last_index_string(),
                         JSRegExp::kLastIndexFieldIndex, writable,
                         Representation::Tagged());
    initial_map->AppendDescriptor(&field);

    static const int num_fields = JSRegExp::kInObjectFieldCount;
    initial_map->SetInObjectProperties(num_fields);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(initial_map->instance_size() +
                                   num_fields * kPointerSize);
  }

  {  // -- E r r o r
    Handle<JSFunction> error_fun = InstallFunction(
        global, "Error", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, error_fun,
                                     Context::ERROR_FUNCTION_INDEX);
  }

  {  // -- E v a l E r r o r
    Handle<JSFunction> eval_error_fun = InstallFunction(
        global, "EvalError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, eval_error_fun,
                                     Context::EVAL_ERROR_FUNCTION_INDEX);
  }

  {  // -- R a n g e E r r o r
    Handle<JSFunction> range_error_fun = InstallFunction(
        global, "RangeError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, range_error_fun,
                                     Context::RANGE_ERROR_FUNCTION_INDEX);
  }

  {  // -- R e f e r e n c e E r r o r
    Handle<JSFunction> reference_error_fun = InstallFunction(
        global, "ReferenceError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, reference_error_fun,
                                     Context::REFERENCE_ERROR_FUNCTION_INDEX);
  }

  {  // -- S y n t a x E r r o r
    Handle<JSFunction> syntax_error_fun = InstallFunction(
        global, "SyntaxError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, syntax_error_fun,
                                     Context::SYNTAX_ERROR_FUNCTION_INDEX);
  }

  {  // -- T y p e E r r o r
    Handle<JSFunction> type_error_fun = InstallFunction(
        global, "TypeError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, type_error_fun,
                                     Context::TYPE_ERROR_FUNCTION_INDEX);
  }

  {  // -- U R I E r r o r
    Handle<JSFunction> uri_error_fun = InstallFunction(
        global, "URIError", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, uri_error_fun,
                                     Context::URI_ERROR_FUNCTION_INDEX);
  }

  // Initialize the embedder data slot.
  Handle<FixedArray> embedder_data = factory->NewFixedArray(3);
  native_context()->set_embedder_data(*embedder_data);

  if (context_type == THIN_CONTEXT) return;

  {  // -- J S O N
    Handle<String> name = factory->InternalizeUtf8String("JSON");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetInstancePrototype(cons,
        Handle<Object>(native_context()->initial_object_prototype(), isolate));
    cons->shared()->set_instance_class_name(*name);
    Handle<JSObject> json_object = factory->NewJSObject(cons, TENURED);
    DCHECK(json_object->IsJSObject());
    JSObject::AddProperty(global, name, json_object, DONT_ENUM);
  }

  {  // -- M a t h
    Handle<String> name = factory->InternalizeUtf8String("Math");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetInstancePrototype(
        cons,
        Handle<Object>(native_context()->initial_object_prototype(), isolate));
    cons->shared()->set_instance_class_name(*name);
    Handle<JSObject> math = factory->NewJSObject(cons, TENURED);
    DCHECK(math->IsJSObject());
    JSObject::AddProperty(global, name, math, DONT_ENUM);
    SimpleInstallFunction(math, "max", Builtins::kMathMax, 2, false);
    SimpleInstallFunction(math, "min", Builtins::kMathMin, 2, false);
  }

  {  // -- A r r a y B u f f e r
    Handle<JSFunction> array_buffer_fun =
        InstallArrayBuffer(global, "ArrayBuffer");
    InstallWithIntrinsicDefaultProto(isolate, array_buffer_fun,
                                     Context::ARRAY_BUFFER_FUN_INDEX);
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

    Handle<JSFunction> data_view_fun = InstallFunction(
        global, "DataView", JS_DATA_VIEW_TYPE,
        JSDataView::kSizeWithInternalFields,
        isolate->initial_object_prototype(), Builtins::kDataViewConstructor);
    InstallWithIntrinsicDefaultProto(isolate, data_view_fun,
                                     Context::DATA_VIEW_FUN_INDEX);
    data_view_fun->shared()->set_construct_stub(
        *isolate->builtins()->DataViewConstructor_ConstructStub());
    data_view_fun->shared()->set_length(3);
    data_view_fun->shared()->DontAdaptArguments();
  }

  {  // -- M a p
    Handle<JSFunction> js_map_fun = InstallFunction(
        global, "Map", JS_MAP_TYPE, JSMap::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_map_fun,
                                     Context::JS_MAP_FUN_INDEX);
  }

  {  // -- S e t
    Handle<JSFunction> js_set_fun = InstallFunction(
        global, "Set", JS_SET_TYPE, JSSet::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    InstallWithIntrinsicDefaultProto(isolate, js_set_fun,
                                     Context::JS_SET_FUN_INDEX);
  }

  {  // -- I t e r a t o r R e s u l t
    Handle<Map> map =
        factory->NewMap(JS_OBJECT_TYPE, JSIteratorResult::kSize);
    Map::SetPrototype(map, isolate->initial_object_prototype());
    Map::EnsureDescriptorSlack(map, 2);

    {  // value
      DataDescriptor d(factory->value_string(), JSIteratorResult::kValueIndex,
                       NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    {  // done
      DataDescriptor d(factory->done_string(), JSIteratorResult::kDoneIndex,
                       NONE, Representation::Tagged());
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

  {  // --- B o u n d F u n c t i o n
    Handle<Map> map =
        factory->NewMap(JS_BOUND_FUNCTION_TYPE, JSBoundFunction::kSize);
    map->set_is_callable();
    Map::SetPrototype(map, empty_function);

    PropertyAttributes roc_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    Map::EnsureDescriptorSlack(map, 2);

    {  // length
      DataDescriptor d(factory->length_string(), JSBoundFunction::kLengthIndex,
                       roc_attribs, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // name
      DataDescriptor d(factory->name_string(), JSBoundFunction::kNameIndex,
                       roc_attribs, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    map->SetInObjectProperties(2);
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
        JS_OBJECT_TYPE, JSSloppyArgumentsObject::kSize, FAST_ELEMENTS);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 2);

    {  // length
      DataDescriptor d(factory->length_string(),
                       JSSloppyArgumentsObject::kLengthIndex, DONT_ENUM,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      DataDescriptor d(factory->callee_string(),
                       JSSloppyArgumentsObject::kCalleeIndex, DONT_ENUM,
                       Representation::Tagged());
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

    // Create the ThrowTypeError functions.
    Handle<AccessorPair> callee = factory->NewAccessorPair();
    Handle<AccessorPair> caller = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetStrictArgumentsPoisonFunction();

    // Install the ThrowTypeError functions.
    callee->set_getter(*poison);
    callee->set_setter(*poison);
    caller->set_getter(*poison);
    caller->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(
        JS_OBJECT_TYPE, JSStrictArgumentsObject::kSize, FAST_ELEMENTS);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 3);

    {  // length
      DataDescriptor d(factory->length_string(),
                       JSStrictArgumentsObject::kLengthIndex, DONT_ENUM,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      AccessorConstantDescriptor d(factory->callee_string(), callee,
                                   attributes);
      map->AppendDescriptor(&d);
    }
    {  // caller
      AccessorConstantDescriptor d(factory->caller_string(), caller,
                                   attributes);
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
  Handle<JSFunction> result = InstallFunction(
      global, name, JS_TYPED_ARRAY_TYPE, JSTypedArray::kSize,
      isolate()->initial_object_prototype(), Builtins::kIllegal);

  Handle<Map> initial_map = isolate()->factory()->NewMap(
      JS_TYPED_ARRAY_TYPE,
      JSTypedArray::kSizeWithInternalFields,
      elements_kind);
  JSFunction::SetInitialMap(result, initial_map,
                            handle(initial_map->prototype(), isolate()));
  *fun = result;
}


void Genesis::InitializeExperimentalGlobal() {
#define FEATURE_INITIALIZE_GLOBAL(id, descr) InitializeGlobal_##id();

  HARMONY_INPROGRESS(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_STAGED(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_SHIPPING(FEATURE_INITIALIZE_GLOBAL)
  FEATURE_INITIALIZE_GLOBAL(promise_extra, "")
#undef FEATURE_INITIALIZE_GLOBAL
}


bool Bootstrapper::CompileBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->SourceLookup<Natives>(index);

  // We pass in extras_utils so that builtin code can set it up for later use
  // by actual extras code, compiled with CompileExtraBuiltin.
  Handle<Object> global = isolate->global_object();
  Handle<Object> utils = isolate->natives_utils_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, utils, extras_utils};

  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, NATIVES_CODE);
}


bool Bootstrapper::CompileExperimentalBuiltin(Isolate* isolate, int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExperimentalNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->SourceLookup<ExperimentalNatives>(index);
  Handle<Object> global = isolate->global_object();
  Handle<Object> utils = isolate->natives_utils_object();
  Handle<Object> args[] = {global, utils};
  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, NATIVES_CODE);
}


bool Bootstrapper::CompileExtraBuiltin(Isolate* isolate, int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExtraNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->SourceLookup<ExtraNatives>(index);
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
      isolate->bootstrapper()->SourceLookup<ExperimentalExtraNatives>(index);
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
  if (check.JsHasOverflowed(1 * KB)) {
    isolate->StackOverflow();
    return false;
  }

  Handle<Context> context(isolate->context());

  Handle<String> script_name =
      isolate->factory()->NewStringFromUtf8(name).ToHandleChecked();
  Handle<SharedFunctionInfo> function_info = Compiler::CompileScript(
      source, script_name, 0, 0, ScriptOriginOptions(), Handle<Object>(),
      context, NULL, NULL, ScriptCompiler::kNoCompileOptions, natives_flag,
      false);
  if (function_info.is_null()) return false;

  DCHECK(context->IsNativeContext());

  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(function_info,
                                                            context);
  Handle<Object> receiver = isolate->factory()->undefined_value();

  // For non-extension scripts, run script to get the function wrapper.
  Handle<Object> wrapper;
  if (!Execution::Call(isolate, fun, receiver, 0, NULL).ToHandle(&wrapper)) {
    return false;
  }
  // Then run the function wrapper.
  return !Execution::Call(isolate, Handle<JSFunction>::cast(wrapper), receiver,
                          argc, argv).is_null();
}


bool Genesis::CallUtilsFunction(Isolate* isolate, const char* name) {
  Handle<JSObject> utils =
      Handle<JSObject>::cast(isolate->natives_utils_object());
  Handle<String> name_string =
      isolate->factory()->NewStringFromAsciiChecked(name);
  Handle<Object> fun = JSObject::GetDataProperty(utils, name_string);
  Handle<Object> receiver = isolate->factory()->undefined_value();
  Handle<Object> args[] = {utils};
  return !Execution::Call(isolate, fun, receiver, 1, args).is_null();
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
    function_info = Compiler::CompileScript(
        source, script_name, 0, 0, ScriptOriginOptions(), Handle<Object>(),
        context, extension, NULL, ScriptCompiler::kNoCompileOptions,
        EXTENSION_CODE, false);
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
  return !Execution::Call(isolate, fun, receiver, 0, NULL).is_null();
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
      Object::GetProperty(global, property_string).ToHandleChecked());
  if (strcmp("prototype", inner) == 0) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(object);
    return Handle<JSObject>(JSObject::cast(function->prototype()));
  }
  Handle<String> inner_string = factory->InternalizeUtf8String(inner);
  DCHECK(!inner_string.is_null());
  Handle<Object> value =
      Object::GetProperty(object, inner_string).ToHandleChecked();
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
    case THIN_CONTEXT:
      break;
  }

  // The utils object can be removed for cases that reach this point.
  native_context()->set_natives_utils_object(heap()->undefined_value());
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

  {
    Handle<JSFunction> to_string = InstallFunction(
        container, "object_to_string", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        MaybeHandle<JSObject>(), Builtins::kObjectProtoToString);
    to_string->shared()->DontAdaptArguments();
    to_string->shared()->set_length(0);
    native_context->set_object_to_string(*to_string);
  }

  Handle<JSObject> iterator_prototype;

  {
    PrototypeIterator iter(native_context->generator_object_prototype_map());
    iter.Advance();  // Advance to the prototype of generator_object_prototype.
    iterator_prototype = Handle<JSObject>(iter.GetCurrent<JSObject>());

    JSObject::AddProperty(container,
                          factory->InternalizeUtf8String("IteratorPrototype"),
                          iterator_prototype, NONE);
  }

  {
    PrototypeIterator iter(native_context->sloppy_generator_function_map());
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
        native_context->sloppy_generator_function_map());
    generator_function_function->shared()->DontAdaptArguments();
    generator_function_function->shared()->set_construct_stub(
        *isolate->builtins()->GeneratorFunctionConstructor());
    generator_function_function->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, generator_function_function,
        Context::GENERATOR_FUNCTION_FUNCTION_INDEX);

    native_context->sloppy_generator_function_map()->SetConstructor(
        *generator_function_function);
    native_context->strict_generator_function_map()->SetConstructor(
        *generator_function_function);
    native_context->strong_generator_function_map()->SetConstructor(
        *generator_function_function);
  }

  {  // -- S e t I t e r a t o r
    Handle<JSObject> set_iterator_prototype =
        isolate->factory()->NewJSObject(isolate->object_function(), TENURED);
    SetObjectPrototype(set_iterator_prototype, iterator_prototype);
    Handle<JSFunction> set_iterator_function = InstallFunction(
        container, "SetIterator", JS_SET_ITERATOR_TYPE, JSSetIterator::kSize,
        set_iterator_prototype, Builtins::kIllegal);
    native_context->set_set_iterator_map(set_iterator_function->initial_map());
  }

  {  // -- M a p I t e r a t o r
    Handle<JSObject> map_iterator_prototype =
        isolate->factory()->NewJSObject(isolate->object_function(), TENURED);
    SetObjectPrototype(map_iterator_prototype, iterator_prototype);
    Handle<JSFunction> map_iterator_function = InstallFunction(
        container, "MapIterator", JS_MAP_ITERATOR_TYPE, JSMapIterator::kSize,
        map_iterator_prototype, Builtins::kIllegal);
    native_context->set_map_iterator_map(map_iterator_function->initial_map());
  }

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun = InstallFunction(
        container, "Script", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    Accessors::FunctionSetPrototype(script_fun, prototype).Assert();
    native_context->set_script_function(*script_fun);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());
    Map::EnsureDescriptorSlack(script_map, 15);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    Handle<AccessorInfo> script_column =
        Accessors::ScriptColumnOffsetInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_column->name())), script_column,
          attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_id = Accessors::ScriptIdInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(Handle<Name>(Name::cast(script_id->name())),
                                   script_id, attribs);
      script_map->AppendDescriptor(&d);
    }


    Handle<AccessorInfo> script_name =
        Accessors::ScriptNameInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_name->name())), script_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_line =
        Accessors::ScriptLineOffsetInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_line->name())), script_line, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source =
        Accessors::ScriptSourceInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_source->name())), script_source,
          attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_type =
        Accessors::ScriptTypeInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_type->name())), script_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_compilation_type =
        Accessors::ScriptCompilationTypeInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_compilation_type->name())),
          script_compilation_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_line_ends =
        Accessors::ScriptLineEndsInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_line_ends->name())), script_line_ends,
          attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_context_data =
        Accessors::ScriptContextDataInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_context_data->name())),
          script_context_data, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script =
        Accessors::ScriptEvalFromScriptInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_script->name())),
          script_eval_from_script, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script_position =
        Accessors::ScriptEvalFromScriptPositionInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_script_position->name())),
          script_eval_from_script_position, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_function_name =
        Accessors::ScriptEvalFromFunctionNameInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_function_name->name())),
          script_eval_from_function_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_url =
        Accessors::ScriptSourceUrlInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_source_url->name())),
          script_source_url, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_mapping_url =
        Accessors::ScriptSourceMappingUrlInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_source_mapping_url->name())),
          script_source_mapping_url, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_is_embedder_debug_script =
        Accessors::ScriptIsEmbedderDebugScriptInfo(isolate, attribs);
    {
      AccessorConstantDescriptor d(
          Handle<Name>(Name::cast(script_is_embedder_debug_script->name())),
          script_is_embedder_debug_script, attribs);
      script_map->AppendDescriptor(&d);
    }
  }
}


void Bootstrapper::ExportExperimentalFromRuntime(Isolate* isolate,
                                                 Handle<JSObject> container) {
  HandleScope scope(isolate);

#define INITIALIZE_FLAG(FLAG)                                         \
  {                                                                   \
    Handle<String> name =                                             \
        isolate->factory()->NewStringFromAsciiChecked(#FLAG);         \
    JSObject::AddProperty(container, name,                            \
                          isolate->factory()->ToBoolean(FLAG), NONE); \
  }

  INITIALIZE_FLAG(FLAG_harmony_tostring)
  INITIALIZE_FLAG(FLAG_harmony_species)

#undef INITIALIZE_FLAG
}


#define EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(id) \
  void Genesis::InitializeGlobal_##id() {}

EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_modules)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_sloppy)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_sloppy_function)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_sloppy_let)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_default_parameters)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_destructuring_bind)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_destructuring_assignment)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_object_observe)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexps)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_unicode_regexps)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_do_expressions)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_iterator_close)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_lookbehind)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_property)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_function_name)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_function_sent)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(promise_extra)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_tailcalls)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_instanceof)

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


void Genesis::InitializeGlobal_harmony_tostring() {
  if (!FLAG_harmony_tostring) return;
  InstallPublicSymbol(factory(), native_context(), "toStringTag",
                      factory()->to_string_tag_symbol());
}


void Genesis::InitializeGlobal_harmony_regexp_subclass() {
  if (!FLAG_harmony_regexp_subclass) return;
  InstallPublicSymbol(factory(), native_context(), "match",
                      factory()->match_symbol());
  InstallPublicSymbol(factory(), native_context(), "replace",
                      factory()->replace_symbol());
  InstallPublicSymbol(factory(), native_context(), "search",
                      factory()->search_symbol());
  InstallPublicSymbol(factory(), native_context(), "split",
                      factory()->split_symbol());
}


void Genesis::InitializeGlobal_harmony_reflect() {
  Factory* factory = isolate()->factory();

  // We currently use some of the Reflect functions internally, even when
  // the --harmony-reflect flag is not given.

  Handle<JSFunction> define_property =
      SimpleCreateFunction(isolate(), factory->defineProperty_string(),
                           Builtins::kReflectDefineProperty, 3, true);
  native_context()->set_reflect_define_property(*define_property);

  Handle<JSFunction> delete_property =
      SimpleCreateFunction(isolate(), factory->deleteProperty_string(),
                           Builtins::kReflectDeleteProperty, 2, true);
  native_context()->set_reflect_delete_property(*delete_property);

  Handle<JSFunction> apply = SimpleCreateFunction(
      isolate(), factory->apply_string(), Builtins::kReflectApply, 3, false);
  native_context()->set_reflect_apply(*apply);

  Handle<JSFunction> construct =
      SimpleCreateFunction(isolate(), factory->construct_string(),
                           Builtins::kReflectConstruct, 2, false);
  native_context()->set_reflect_construct(*construct);

  if (!FLAG_harmony_reflect) return;

  Handle<JSGlobalObject> global(JSGlobalObject::cast(
      native_context()->global_object()));
  Handle<String> reflect_string = factory->NewStringFromStaticChars("Reflect");
  Handle<JSObject> reflect =
      factory->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::AddProperty(global, reflect_string, reflect, DONT_ENUM);

  InstallFunction(reflect, define_property, factory->defineProperty_string());
  InstallFunction(reflect, delete_property, factory->deleteProperty_string());
  InstallFunction(reflect, apply, factory->apply_string());
  InstallFunction(reflect, construct, factory->construct_string());

  SimpleInstallFunction(reflect, factory->get_string(),
                        Builtins::kReflectGet, 2, false);
  SimpleInstallFunction(reflect, factory->getOwnPropertyDescriptor_string(),
                        Builtins::kReflectGetOwnPropertyDescriptor, 2, true);
  SimpleInstallFunction(reflect, factory->getPrototypeOf_string(),
                        Builtins::kReflectGetPrototypeOf, 1, true);
  SimpleInstallFunction(reflect, factory->has_string(),
                        Builtins::kReflectHas, 2, true);
  SimpleInstallFunction(reflect, factory->isExtensible_string(),
                        Builtins::kReflectIsExtensible, 1, true);
  SimpleInstallFunction(reflect, factory->ownKeys_string(),
                        Builtins::kReflectOwnKeys, 1, true);
  SimpleInstallFunction(reflect, factory->preventExtensions_string(),
                        Builtins::kReflectPreventExtensions, 1, true);
  SimpleInstallFunction(reflect, factory->set_string(),
                        Builtins::kReflectSet, 3, false);
  SimpleInstallFunction(reflect, factory->setPrototypeOf_string(),
                        Builtins::kReflectSetPrototypeOf, 2, true);
}


void Genesis::InitializeGlobal_harmony_sharedarraybuffer() {
  if (!FLAG_harmony_sharedarraybuffer) return;

  Handle<JSGlobalObject> global(native_context()->global_object());
  Handle<JSFunction> shared_array_buffer_fun =
      InstallArrayBuffer(global, "SharedArrayBuffer");
  native_context()->set_shared_array_buffer_fun(*shared_array_buffer_fun);
}


void Genesis::InitializeGlobal_harmony_simd() {
  if (!FLAG_harmony_simd) return;

  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(native_context()->global_object()));
  Isolate* isolate = global->GetIsolate();
  Factory* factory = isolate->factory();

  Handle<String> name = factory->InternalizeUtf8String("SIMD");
  Handle<JSFunction> cons = factory->NewFunction(name);
  JSFunction::SetInstancePrototype(
      cons,
      Handle<Object>(native_context()->initial_object_prototype(), isolate));
  cons->shared()->set_instance_class_name(*name);
  Handle<JSObject> simd_object = factory->NewJSObject(cons, TENURED);
  DCHECK(simd_object->IsJSObject());
  JSObject::AddProperty(global, name, simd_object, DONT_ENUM);

// Install SIMD type functions. Set the instance class names since
// InstallFunction only does this when we install on the JSGlobalObject.
#define SIMD128_INSTALL_FUNCTION(TYPE, Type, type, lane_count, lane_type) \
  Handle<JSFunction> type##_function = InstallFunction(                   \
      simd_object, #Type, JS_VALUE_TYPE, JSValue::kSize,                  \
      isolate->initial_object_prototype(), Builtins::kIllegal);           \
  native_context()->set_##type##_function(*type##_function);              \
  type##_function->shared()->set_instance_class_name(*factory->Type##_string());
  SIMD128_TYPES(SIMD128_INSTALL_FUNCTION)
#undef SIMD128_INSTALL_FUNCTION
}


void Genesis::InitializeGlobal_harmony_object_values_entries() {
  if (!FLAG_harmony_object_values_entries) return;

  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(native_context()->global_object()));
  Isolate* isolate = global->GetIsolate();
  Factory* factory = isolate->factory();

  Handle<JSFunction> object_function = isolate->object_function();
  SimpleInstallFunction(object_function, factory->entries_string(),
                        Builtins::kObjectEntries, 1, false);
  SimpleInstallFunction(object_function, factory->values_string(),
                        Builtins::kObjectValues, 1, false);
}

void Genesis::InitializeGlobal_harmony_object_own_property_descriptors() {
  if (!FLAG_harmony_object_own_property_descriptors) return;

  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(native_context()->global_object()));
  Isolate* isolate = global->GetIsolate();
  Factory* factory = isolate->factory();

  Handle<JSFunction> object_function = isolate->object_function();
  SimpleInstallFunction(object_function,
                        factory->getOwnPropertyDescriptors_string(),
                        Builtins::kObjectGetOwnPropertyDescriptors, 1, false);
}

void Genesis::InstallJSProxyMaps() {
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


void Genesis::InitializeGlobal_harmony_proxies() {
  if (!FLAG_harmony_proxies) return;
  Handle<JSGlobalObject> global(
      JSGlobalObject::cast(native_context()->global_object()));
  Isolate* isolate = global->GetIsolate();
  Factory* factory = isolate->factory();

  InstallJSProxyMaps();

  // Create the Proxy object.
  Handle<String> name = factory->Proxy_string();
  Handle<Code> code(isolate->builtins()->ProxyConstructor());

  Handle<JSFunction> proxy_function =
      factory->NewFunction(isolate->proxy_function_map(),
                           factory->Proxy_string(), MaybeHandle<Code>(code));

  JSFunction::SetInitialMap(proxy_function,
                            Handle<Map>(native_context()->proxy_map(), isolate),
                            factory->null_value());

  proxy_function->shared()->set_construct_stub(
      *isolate->builtins()->ProxyConstructor_ConstructStub());
  proxy_function->shared()->set_internal_formal_parameter_count(2);
  proxy_function->shared()->set_length(2);

  native_context()->set_proxy_function(*proxy_function);
  InstallFunction(global, name, proxy_function, factory->Object_string());
}


Handle<JSFunction> Genesis::InstallArrayBuffer(Handle<JSObject> target,
                                               const char* name) {
  // Setup the {prototype} with the given {name} for @@toStringTag.
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::AddProperty(prototype, factory()->to_string_tag_symbol(),
                        factory()->NewStringFromAsciiChecked(name),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // Allocate the constructor with the given {prototype}.
  Handle<JSFunction> array_buffer_fun =
      InstallFunction(target, name, JS_ARRAY_BUFFER_TYPE,
                      JSArrayBuffer::kSizeWithInternalFields, prototype,
                      Builtins::kArrayBufferConstructor);
  array_buffer_fun->shared()->set_construct_stub(
      *isolate()->builtins()->ArrayBufferConstructor_ConstructStub());
  array_buffer_fun->shared()->DontAdaptArguments();
  array_buffer_fun->shared()->set_length(1);

  // Install the "constructor" property on the {prototype}.
  JSObject::AddProperty(prototype, factory()->constructor_string(),
                        array_buffer_fun, DONT_ENUM);

  SimpleInstallFunction(array_buffer_fun, factory()->isView_string(),
                        Builtins::kArrayBufferIsView, 1, true);

  return array_buffer_fun;
}


void Genesis::InitializeGlobal_harmony_species() {
  if (!FLAG_harmony_species) return;
  InstallPublicSymbol(factory(), native_context(), "species",
                      factory()->species_symbol());
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
  array_function->shared()->set_construct_stub(*code);
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
    AccessorConstantDescriptor d(Handle<Name>(Name::cast(array_length->name())),
                                 array_length, attribs);
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

  int builtin_index = Natives::GetDebuggerCount();
  // Only run prologue.js and runtime.js at this point.
  DCHECK_EQ(builtin_index, Natives::GetIndex("prologue"));
  if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;
  DCHECK_EQ(builtin_index, Natives::GetIndex("runtime"));
  if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;

  // A thin context is ready at this point.
  if (context_type == THIN_CONTEXT) return true;

  {
    // Builtin function for OpaqueReference -- a JSValue-based object,
    // that keeps its field isolated from JavaScript code. It may store
    // objects, that JavaScript code may not access.
    Handle<JSFunction> opaque_reference_fun = factory()->NewFunction(
        factory()->empty_string(), isolate()->builtins()->Illegal(),
        isolate()->initial_object_prototype(), JS_VALUE_TYPE, JSValue::kSize);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Accessors::FunctionSetPrototype(opaque_reference_fun, prototype).Assert();
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

  auto template_instantiations_cache =
      ObjectHashTable::New(isolate(), ApiNatives::kInitialFunctionCacheSize,
                           USE_CUSTOM_MINIMUM_CAPACITY);
  native_context()->set_template_instantiations_cache(
      *template_instantiations_cache);

  // Store the map for the %ObjectPrototype% after the natives has been compiled
  // and the Object function has been set up.
  Handle<JSFunction> object_function(native_context()->object_function());
  DCHECK(JSObject::cast(object_function->initial_map()->prototype())
             ->HasFastProperties());
  native_context()->set_object_function_prototype_map(
      HeapObject::cast(object_function->initial_map()->prototype())->map());

  // Store the map for the %StringPrototype% after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function());
  DCHECK(JSObject::cast(
      string_function->initial_map()->prototype())->HasFastProperties());
  native_context()->set_string_function_prototype_map(
      HeapObject::cast(string_function->initial_map()->prototype())->map());

  // Install Global.eval.
  {
    Handle<JSFunction> eval = SimpleInstallFunction(
        handle(native_context()->global_object()), factory()->eval_string(),
        Builtins::kGlobalEval, 1, false);
    native_context()->set_global_eval_fun(*eval);
  }

  // Install Array.prototype.concat
  {
    Handle<JSFunction> array_constructor(native_context()->array_function());
    Handle<JSObject> proto(JSObject::cast(array_constructor->prototype()));
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

  // Set up the Promise constructor.
  {
    Handle<String> key = factory()->Promise_string();
    Handle<JSFunction> function = Handle<JSFunction>::cast(
        Object::GetProperty(handle(native_context()->global_object()), key)
            .ToHandleChecked());
    JSFunction::EnsureHasInitialMap(function);
    function->initial_map()->set_instance_type(JS_PROMISE_TYPE);
    function->shared()->set_construct_stub(
        *isolate()->builtins()->JSBuiltinsConstructStub());
    InstallWithIntrinsicDefaultProto(isolate(), function,
                                     Context::PROMISE_FUNCTION_INDEX);
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
      DataDescriptor d(factory()->get_string(),
                       JSAccessorPropertyDescriptor::kGetIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // set
      DataDescriptor d(factory()->set_string(),
                       JSAccessorPropertyDescriptor::kSetIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      DataDescriptor d(factory()->enumerable_string(),
                       JSAccessorPropertyDescriptor::kEnumerableIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      DataDescriptor d(factory()->configurable_string(),
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
      DataDescriptor d(factory()->value_string(),
                       JSDataPropertyDescriptor::kValueIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // writable
      DataDescriptor d(factory()->writable_string(),
                       JSDataPropertyDescriptor::kWritableIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      DataDescriptor d(factory()->enumerable_string(),
                       JSDataPropertyDescriptor::kEnumerableIndex, NONE,
                       Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      DataDescriptor d(factory()->configurable_string(),
                       JSDataPropertyDescriptor::kConfigurableIndex, NONE,
                       Representation::Tagged());
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
      AccessorConstantDescriptor desc(
          length, handle(array_descriptors->GetValue(old), isolate()),
          array_descriptors->GetDetails(old).attributes());
      initial_map->AppendDescriptor(&desc);
    }
    {
      DataDescriptor index_field(factory()->index_string(),
                                 JSRegExpResult::kIndexIndex, NONE,
                                 Representation::Tagged());
      initial_map->AppendDescriptor(&index_field);
    }

    {
      DataDescriptor input_field(factory()->input_string(),
                                 JSRegExpResult::kInputIndex, NONE,
                                 Representation::Tagged());
      initial_map->AppendDescriptor(&input_field);
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
      AccessorConstantDescriptor d(factory()->iterator_symbol(),
                                   arguments_iterator, attribs);
      Handle<Map> map(native_context()->sloppy_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      AccessorConstantDescriptor d(factory()->iterator_symbol(),
                                   arguments_iterator, attribs);
      Handle<Map> map(native_context()->fast_aliased_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      AccessorConstantDescriptor d(factory()->iterator_symbol(),
                                   arguments_iterator, attribs);
      Handle<Map> map(native_context()->slow_aliased_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
    {
      AccessorConstantDescriptor d(factory()->iterator_symbol(),
                                   arguments_iterator, attribs);
      Handle<Map> map(native_context()->strict_arguments_map());
      Map::EnsureDescriptorSlack(map, 1);
      map->AppendDescriptor(&d);
    }
  }

  return true;
}


bool Genesis::InstallExperimentalNatives() {
  static const char* harmony_proxies_natives[] = {"native proxy.js", nullptr};
  static const char* harmony_modules_natives[] = {nullptr};
  static const char* harmony_regexps_natives[] = {"native harmony-regexp.js",
                                                  nullptr};
  static const char* harmony_tostring_natives[] = {nullptr};
  static const char* harmony_iterator_close_natives[] = {nullptr};
  static const char* harmony_sloppy_natives[] = {nullptr};
  static const char* harmony_sloppy_function_natives[] = {nullptr};
  static const char* harmony_sloppy_let_natives[] = {nullptr};
  static const char* harmony_species_natives[] = {"native harmony-species.js",
                                                  nullptr};
  static const char* harmony_tailcalls_natives[] = {nullptr};
  static const char* harmony_unicode_regexps_natives[] = {
      "native harmony-unicode-regexps.js", nullptr};
  static const char* harmony_default_parameters_natives[] = {nullptr};
  static const char* harmony_reflect_natives[] = {"native harmony-reflect.js",
                                                  nullptr};
  static const char* harmony_destructuring_bind_natives[] = {nullptr};
  static const char* harmony_destructuring_assignment_natives[] = {nullptr};
  static const char* harmony_object_observe_natives[] = {
      "native harmony-object-observe.js", nullptr};
  static const char* harmony_sharedarraybuffer_natives[] = {
      "native harmony-sharedarraybuffer.js", "native harmony-atomics.js", NULL};
  static const char* harmony_simd_natives[] = {"native harmony-simd.js",
                                               nullptr};
  static const char* harmony_do_expressions_natives[] = {nullptr};
  static const char* harmony_regexp_subclass_natives[] = {nullptr};
  static const char* harmony_regexp_lookbehind_natives[] = {nullptr};
  static const char* harmony_instanceof_natives[] = {nullptr};
  static const char* harmony_regexp_property_natives[] = {nullptr};
  static const char* harmony_function_name_natives[] = {nullptr};
  static const char* harmony_function_sent_natives[] = {nullptr};
  static const char* promise_extra_natives[] = {"native promise-extra.js",
                                                nullptr};
  static const char* harmony_object_values_entries_natives[] = {nullptr};
  static const char* harmony_object_own_property_descriptors_natives[] = {
      nullptr};

  for (int i = ExperimentalNatives::GetDebuggerCount();
       i < ExperimentalNatives::GetBuiltinsCount(); i++) {
#define INSTALL_EXPERIMENTAL_NATIVES(id, desc)                                \
  if (FLAG_##id) {                                                            \
    for (size_t j = 0; id##_natives[j] != NULL; j++) {                        \
      Vector<const char> script_name = ExperimentalNatives::GetScriptName(i); \
      if (strncmp(script_name.start(), id##_natives[j],                       \
                  script_name.length()) == 0) {                               \
        if (!Bootstrapper::CompileExperimentalBuiltin(isolate(), i)) {        \
          return false;                                                       \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  }
    HARMONY_INPROGRESS(INSTALL_EXPERIMENTAL_NATIVES);
    HARMONY_STAGED(INSTALL_EXPERIMENTAL_NATIVES);
    HARMONY_SHIPPING(INSTALL_EXPERIMENTAL_NATIVES);
    INSTALL_EXPERIMENTAL_NATIVES(promise_extra, "");
#undef INSTALL_EXPERIMENTAL_NATIVES
  }

  if (!CallUtilsFunction(isolate(), "PostExperimentals")) return false;

  InstallExperimentalBuiltinFunctionIds();
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
  return CallUtilsFunction(isolate(), "PostDebug");
}


static void InstallBuiltinFunctionId(Handle<JSObject> holder,
                                     const char* function_name,
                                     BuiltinFunctionId id) {
  Isolate* isolate = holder->GetIsolate();
  Handle<Object> function_object =
      Object::GetProperty(isolate, holder, function_name).ToHandleChecked();
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  function->shared()->set_function_data(Smi::FromInt(id));
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


void Genesis::InstallExperimentalBuiltinFunctionIds() {
  if (FLAG_harmony_sharedarraybuffer) {
    struct BuiltinFunctionIds {
      const char* holder_expr;
      const char* fun_name;
      BuiltinFunctionId id;
    };

    const BuiltinFunctionIds atomic_builtins[] = {
        ATOMIC_FUNCTIONS_WITH_ID_LIST(INSTALL_BUILTIN_ID)};

    for (const BuiltinFunctionIds& builtin : atomic_builtins) {
      Handle<JSObject> holder =
          ResolveBuiltinIdHolder(native_context(), builtin.holder_expr);
      InstallBuiltinFunctionId(holder, builtin.fun_name, builtin.id);
    }
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
  Handle<JSGlobalObject> global(JSGlobalObject::cast(
      native_context->global_object()));

  Handle<JSObject> Error = isolate->error_function();
  Handle<String> name =
      factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("stackTraceLimit"));
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(Error, name, stack_trace_limit, NONE);

  // Expose the debug global object in global if a name for it is specified.
  if (FLAG_expose_debug_as != NULL && strlen(FLAG_expose_debug_as) != 0) {
    // If loading fails we just bail out without installing the
    // debugger but without tanking the whole context.
    Debug* debug = isolate->debug();
    if (!debug->Load()) return true;
    Handle<Context> debug_context = debug->debug_context();
    // Set the security token for the debugger context to the same as
    // the shell native context to allow calling between these (otherwise
    // exposing debug global object doesn't make much sense).
    debug_context->set_security_token(native_context->security_token());
    Handle<String> debug_string =
        factory->InternalizeUtf8String(FLAG_expose_debug_as);
    uint32_t index;
    if (debug_string->AsArrayIndex(&index)) return true;
    Handle<Object> global_proxy(debug_context->global_proxy(), isolate);
    JSObject::AddProperty(global, debug_string, global_proxy, DONT_ENUM);
  }

  if (FLAG_expose_wasm) {
    WasmJs::Install(isolate, global);
  }

  return true;
}


static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}


Genesis::ExtensionStates::ExtensionStates() : map_(HashMap::PointersMatch, 8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  i::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension));
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
      (!FLAG_track_gc_object_stats ||
       InstallExtension(isolate, "v8/statistics", &extension_states)) &&
      (!FLAG_expose_trigger_failure ||
       InstallExtension(isolate, "v8/trigger-failure", &extension_states)) &&
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
  isolate->NotifyExtensionInstalled();
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
    if (!proxy_constructor->prototype_template()->IsUndefined()) {
      Handle<ObjectTemplateInfo> global_object_data(
          ObjectTemplateInfo::cast(proxy_constructor->prototype_template()));
      if (!ConfigureApiObject(global_object, global_object_data)) return false;
    }
  }

  SetObjectPrototype(global_proxy, global_object);

  native_context()->set_initial_array_prototype(
      JSArray::cast(native_context()->array_function()->prototype()));
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
      switch (details.type()) {
        case DATA: {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          DCHECK(!descs->GetDetails(i).representation().IsDouble());
          Handle<Object> value = Handle<Object>(from->RawFastPropertyAt(index),
                                                isolate());
          JSObject::AddProperty(to, key, value, details.attributes());
          break;
        }
        case DATA_CONSTANT: {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          Handle<Object> constant(descs->GetConstant(i), isolate());
          JSObject::AddProperty(to, key, constant, details.attributes());
          break;
        }
        case ACCESSOR:
          UNREACHABLE();
        case ACCESSOR_CONSTANT: {
          Handle<Name> key(descs->GetKey(i));
          LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
          CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
          // If the property is already there we skip it
          if (it.IsFound()) continue;
          HandleScope inner(isolate());
          DCHECK(!to->HasFastProperties());
          // Add to dictionary.
          Handle<Object> callbacks(descs->GetCallbacksObject(i), isolate());
          PropertyDetails d(details.attributes(), ACCESSOR_CONSTANT, i + 1,
                            PropertyCellType::kMutable);
          JSObject::SetNormalizedProperty(to, key, callbacks, d);
          break;
        }
      }
    }
  } else if (from->IsJSGlobalObject()) {
    Handle<GlobalDictionary> properties =
        Handle<GlobalDictionary>(from->global_dictionary());
    int capacity = properties->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* raw_key(properties->KeyAt(i));
      if (properties->IsKey(raw_key)) {
        DCHECK(raw_key->IsName());
        // If the property is already there we skip it.
        Handle<Name> key(Name::cast(raw_key));
        LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
        CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
        if (it.IsFound()) continue;
        // Set the property.
        DCHECK(properties->ValueAt(i)->IsPropertyCell());
        Handle<PropertyCell> cell(PropertyCell::cast(properties->ValueAt(i)));
        Handle<Object> value(cell->value(), isolate());
        if (value->IsTheHole()) continue;
        PropertyDetails details = cell->property_details();
        DCHECK_EQ(kData, details.kind());
        JSObject::AddProperty(to, key, value, details.attributes());
      }
    }
  } else {
    Handle<NameDictionary> properties =
        Handle<NameDictionary>(from->property_dictionary());
    int capacity = properties->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* raw_key(properties->KeyAt(i));
      if (properties->IsKey(raw_key)) {
        DCHECK(raw_key->IsName());
        // If the property is already there we skip it.
        Handle<Name> key(Name::cast(raw_key));
        LookupIterator it(to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
        CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
        if (it.IsFound()) continue;
        // Set the property.
        Handle<Object> value = Handle<Object>(properties->ValueAt(i),
                                              isolate());
        DCHECK(!value->IsCell());
        DCHECK(!value->IsTheHole());
        PropertyDetails details = properties->DetailsAt(i);
        DCHECK_EQ(kData, details.kind());
        JSObject::AddProperty(to, key, value, details.attributes());
      }
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
  SetObjectPrototype(to, proto);
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


class NoTrackDoubleFieldsForSerializerScope {
 public:
  explicit NoTrackDoubleFieldsForSerializerScope(Isolate* isolate)
      : flag_(FLAG_track_double_fields), enabled_(false) {
    if (isolate->serializer_enabled()) {
      // Disable tracking double fields because heap numbers treated as
      // immutable by the serializer.
      FLAG_track_double_fields = false;
      enabled_ = true;
    }
  }

  ~NoTrackDoubleFieldsForSerializerScope() {
    if (enabled_) {
      FLAG_track_double_fields = flag_;
    }
  }

 private:
  bool flag_;
  bool enabled_;
};

Genesis::Genesis(Isolate* isolate,
                 MaybeHandle<JSGlobalProxy> maybe_global_proxy,
                 v8::Local<v8::ObjectTemplate> global_proxy_template,
                 v8::ExtensionConfiguration* extensions,
                 GlobalContextType context_type)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  NoTrackDoubleFieldsForSerializerScope disable_scope(isolate);
  result_ = Handle<Context>::null();
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
    global_proxy = isolate->factory()->NewUninitializedJSGlobalProxy();
  }

  // We can only de-serialize a context if the isolate was initialized from
  // a snapshot. Otherwise we have to build the context from scratch.
  // Also create a context from scratch to expose natives, if required by flag.
  if (!isolate->initialized_from_snapshot() ||
      !Snapshot::NewContextFromSnapshot(isolate, global_proxy)
           .ToHandle(&native_context_)) {
    native_context_ = Handle<Context>();
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(*native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();
#if TRACE_MAPS
    if (FLAG_trace_maps) {
      Handle<JSFunction> object_fun = isolate->object_function();
      PrintF("[TraceMap: InitialMap map= %p SFI= %d_Object ]\n",
             reinterpret_cast<void*>(object_fun->initial_map()),
             object_fun->shared()->unique_id());
      Map::TraceAllTransitions(object_fun->initial_map());
    }
#endif
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);

    HookUpGlobalProxy(global_object, global_proxy);
    HookUpGlobalObject(global_object);

    if (!ConfigureGlobalObjects(global_proxy_template)) return;
  } else {
    // We get here if there was no context snapshot.
    CreateRoots();
    Handle<JSFunction> empty_function = CreateEmptyFunction(isolate);
    CreateStrictModeFunctionMaps(empty_function);
    CreateStrongModeFunctionMaps(empty_function);
    CreateIteratorMaps();
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);
    HookUpGlobalProxy(global_object, global_proxy);
    InitializeGlobal(global_object, empty_function, context_type);
    InitializeNormalizedMapCaches();

    if (!InstallNatives(context_type)) return;

    MakeFunctionInstancePrototypeWritable();

    if (context_type != THIN_CONTEXT) {
      if (!InstallExtraNatives()) return;
      if (!ConfigureGlobalObjects(global_proxy_template)) return;
    }
    isolate->counters()->contexts_created_from_scratch()->Increment();
    // Re-initialize the counter because it got incremented during snapshot
    // creation.
    isolate->native_context()->set_errors_thrown(Smi::FromInt(0));
  }

  // Install experimental natives. Do not include them into the
  // snapshot as we should be able to turn them off at runtime. Re-installing
  // them after they have already been deserialized would also fail.
  if (context_type == FULL_CONTEXT) {
    if (!isolate->serializer_enabled()) {
      InitializeExperimentalGlobal();
      if (!InstallExperimentalNatives()) return;

      if (FLAG_experimental_extras) {
        if (!InstallExperimentalExtraNatives()) return;
      }
    }
    // The serializer cannot serialize typed arrays. Reset those typed arrays
    // for each new context.
  } else if (context_type == DEBUG_CONTEXT) {
    DCHECK(!isolate->serializer_enabled());
    InitializeExperimentalGlobal();
    if (!InstallDebuggerNatives()) return;
  }

  ConfigureUtilsObject(context_type);

  // Check that the script context table is empty except for the 'this' binding.
  // We do not need script contexts for native scripts.
  if (!FLAG_global_var_shortcuts) {
    DCHECK_EQ(1, native_context()->script_context_table()->used());
  }

  result_ = native_context();
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

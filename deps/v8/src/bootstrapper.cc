// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"

#include "src/accessors.h"
#include "src/code-stubs.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/free-buffer-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#include "src/isolate-inl.h"
#include "src/natives.h"
#include "src/snapshot.h"
#include "third_party/fdlibm/fdlibm.h"

namespace v8 {
namespace internal {

NativesExternalStringResource::NativesExternalStringResource(
    Bootstrapper* bootstrapper,
    const char* source,
    size_t length)
    : data_(source), length_(length) {
  if (bootstrapper->delete_these_non_arrays_on_tear_down_ == NULL) {
    bootstrapper->delete_these_non_arrays_on_tear_down_ = new List<char*>(2);
  }
  // The resources are small objects and we only make a fixed number of
  // them, but let's clean them up on exit for neatness.
  bootstrapper->delete_these_non_arrays_on_tear_down_->
      Add(reinterpret_cast<char*>(this));
}


Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION),
      delete_these_non_arrays_on_tear_down_(NULL),
      delete_these_arrays_on_tear_down_(NULL) {
}


Handle<String> Bootstrapper::NativesSourceLookup(int index) {
  DCHECK(0 <= index && index < Natives::GetBuiltinsCount());
  Heap* heap = isolate_->heap();
  if (heap->natives_source_cache()->get(index)->IsUndefined()) {
    // We can use external strings for the natives.
    Vector<const char> source = Natives::GetRawScriptSource(index);
    NativesExternalStringResource* resource =
        new NativesExternalStringResource(this,
                                          source.start(),
                                          source.length());
    // We do not expect this to throw an exception. Change this if it does.
    Handle<String> source_code =
        isolate_->factory()->NewExternalStringFromAscii(
            resource).ToHandleChecked();
    heap->natives_source_cache()->set(index, *source_code);
  }
  Handle<Object> cached_source(heap->natives_source_cache()->get(index),
                               isolate_);
  return Handle<String>::cast(cached_source);
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
  delete gc_extension_;
  delete externalize_string_extension_;
  delete statistics_extension_;
  delete trigger_failure_extension_;
}


char* Bootstrapper::AllocateAutoDeletedArray(int bytes) {
  char* memory = new char[bytes];
  if (memory != NULL) {
    if (delete_these_arrays_on_tear_down_ == NULL) {
      delete_these_arrays_on_tear_down_ = new List<char*>(2);
    }
    delete_these_arrays_on_tear_down_->Add(memory);
  }
  return memory;
}


void Bootstrapper::TearDown() {
  if (delete_these_non_arrays_on_tear_down_ != NULL) {
    int len = delete_these_non_arrays_on_tear_down_->length();
    DCHECK(len < 27);  // Don't use this mechanism for unbounded allocations.
    for (int i = 0; i < len; i++) {
      delete delete_these_non_arrays_on_tear_down_->at(i);
      delete_these_non_arrays_on_tear_down_->at(i) = NULL;
    }
    delete delete_these_non_arrays_on_tear_down_;
    delete_these_non_arrays_on_tear_down_ = NULL;
  }

  if (delete_these_arrays_on_tear_down_ != NULL) {
    int len = delete_these_arrays_on_tear_down_->length();
    DCHECK(len < 1000);  // Don't use this mechanism for unbounded allocations.
    for (int i = 0; i < len; i++) {
      delete[] delete_these_arrays_on_tear_down_->at(i);
      delete_these_arrays_on_tear_down_->at(i) = NULL;
    }
    delete delete_these_arrays_on_tear_down_;
    delete_these_arrays_on_tear_down_ = NULL;
  }

  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}


class Genesis BASE_EMBEDDED {
 public:
  Genesis(Isolate* isolate,
          MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Handle<v8::ObjectTemplate> global_proxy_template,
          v8::ExtensionConfiguration* extensions);
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
  Handle<JSFunction> GetStrictPoisonFunction();
  // Poison for sloppy generator function arguments/callee.
  Handle<JSFunction> GetGeneratorPoisonFunction();

  void CreateStrictModeFunctionMaps(Handle<JSFunction> empty);

  // Make the "arguments" and "caller" properties throw a TypeError on access.
  void PoisonArgumentsAndCaller(Handle<Map> map);

  // Creates the global objects using the global and the template passed in
  // through the API.  We call this regardless of whether we are building a
  // context from scratch or using a deserialized one from the partial snapshot
  // but in the latter case we don't use the objects it produces directly, as
  // we have to used the deserialized ones that are linked together with the
  // rest of the context snapshot.
  Handle<JSGlobalProxy> CreateNewGlobals(
      v8::Handle<v8::ObjectTemplate> global_proxy_template,
      MaybeHandle<JSGlobalProxy> maybe_global_proxy,
      Handle<GlobalObject>* global_object_out);
  // Hooks the given global proxy into the context.  If the context was created
  // by deserialization then this will unhook the global proxy that was
  // deserialized, leaving the GC to pick it up.
  void HookUpGlobalProxy(Handle<GlobalObject> global_object,
                         Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the global that has been created by the templates
  // passed through the API.  The global from the snapshot is detached from the
  // other objects in the snapshot.
  void HookUpGlobalObject(Handle<GlobalObject> global_object);
  // New context initialization.  Used for creating a context from scratch.
  void InitializeGlobal(Handle<GlobalObject> global_object,
                        Handle<JSFunction> empty_function);
  void InitializeExperimentalGlobal();
  // Installs the contents of the native .js files on the global objects.
  // Used for creating a context from scratch.
  void InstallNativeFunctions();
  void InstallExperimentalNativeFunctions();
  Handle<JSFunction> InstallInternalArray(Handle<JSBuiltinsObject> builtins,
                                          const char* name,
                                          ElementsKind elements_kind);
  bool InstallNatives();

  void InstallTypedArray(
      const char* name,
      ElementsKind elements_kind,
      Handle<JSFunction>* fun,
      Handle<Map>* external_map);
  bool InstallExperimentalNatives();
  void InstallBuiltinFunctionIds();
  void InstallJSFunctionResultCaches();
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
  bool InstallJSBuiltins(Handle<JSBuiltinsObject> builtins);
  bool ConfigureApiObject(Handle<JSObject> object,
                          Handle<ObjectTemplateInfo> object_template);
  bool ConfigureGlobalObjects(
      v8::Handle<v8::ObjectTemplate> global_proxy_template);

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
    FUNCTION_WITHOUT_PROTOTYPE,
    BOUND_FUNCTION
  };

  static bool IsFunctionModeWithPrototype(FunctionMode function_mode) {
    return (function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ||
            function_mode == FUNCTION_WITH_READONLY_PROTOTYPE);
  }

  Handle<Map> CreateFunctionMap(FunctionMode function_mode);

  void SetFunctionInstanceDescriptor(Handle<Map> map,
                                     FunctionMode function_mode);
  void MakeFunctionInstancePrototypeWritable();

  Handle<Map> CreateStrictFunctionMap(
      FunctionMode function_mode,
      Handle<JSFunction> empty_function);

  void SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                           FunctionMode function_mode);

  static bool CompileBuiltin(Isolate* isolate, int index);
  static bool CompileExperimentalBuiltin(Isolate* isolate, int index);
  static bool CompileNative(Isolate* isolate,
                            Vector<const char> name,
                            Handle<String> source);
  static bool CompileScriptCached(Isolate* isolate,
                                  Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<Context> native_context_;

  // Function maps. Function maps are created initially with a read only
  // prototype for the processing of JS builtins. Later the function maps are
  // replaced in order to make prototype writable. These are the final, writable
  // prototype, maps.
  Handle<Map> sloppy_function_map_writable_prototype_;
  Handle<Map> strict_function_map_writable_prototype_;
  Handle<JSFunction> strict_poison_function;
  Handle<JSFunction> generator_poison_function;

  BootstrapperActive active_;
  friend class Bootstrapper;
};


void Bootstrapper::Iterate(ObjectVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}


Handle<Context> Bootstrapper::CreateEnvironment(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Handle<v8::ObjectTemplate> global_proxy_template,
    v8::ExtensionConfiguration* extensions) {
  HandleScope scope(isolate_);
  Genesis genesis(
      isolate_, maybe_global_proxy, global_proxy_template, extensions);
  Handle<Context> env = genesis.result();
  if (env.is_null() || !InstallExtensions(env, extensions)) {
    return Handle<Context>();
  }
  return scope.CloseAndEscape(env);
}


static void SetObjectPrototype(Handle<JSObject> object, Handle<Object> proto) {
  // object.__proto__ = proto;
  Handle<Map> old_map = Handle<Map>(object->map());
  Handle<Map> new_map = Map::Copy(old_map);
  new_map->set_prototype(*proto);
  JSObject::MigrateToMap(object, new_map);
}


void Bootstrapper::DetachGlobal(Handle<Context> env) {
  Factory* factory = env->GetIsolate()->factory();
  Handle<JSGlobalProxy> global_proxy(JSGlobalProxy::cast(env->global_proxy()));
  global_proxy->set_native_context(*factory->null_value());
  SetObjectPrototype(global_proxy, factory->null_value());
  global_proxy->map()->set_constructor(*factory->null_value());
}


static Handle<JSFunction> InstallFunction(Handle<JSObject> target,
                                          const char* name,
                                          InstanceType type,
                                          int instance_size,
                                          MaybeHandle<JSObject> maybe_prototype,
                                          Builtins::Name call) {
  Isolate* isolate = target->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<String> internalized_name = factory->InternalizeUtf8String(name);
  Handle<Code> call_code = Handle<Code>(isolate->builtins()->builtin(call));
  Handle<JSObject> prototype;
  Handle<JSFunction> function = maybe_prototype.ToHandle(&prototype)
      ? factory->NewFunction(internalized_name, call_code, prototype,
                             type, instance_size)
      : factory->NewFunctionWithoutPrototype(internalized_name, call_code);
  PropertyAttributes attributes;
  if (target->IsJSBuiltinsObject()) {
    attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  } else {
    attributes = DONT_ENUM;
  }
  JSObject::AddProperty(target, internalized_name, function, attributes);
  if (target->IsJSGlobalObject()) {
    function->shared()->set_instance_class_name(*internalized_name);
  }
  function->shared()->set_native(true);
  return function;
}


void Genesis::SetFunctionInstanceDescriptor(
    Handle<Map> map, FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 5 : 4;
  Map::EnsureDescriptorSlack(map, size);

  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE | READ_ONLY);

  Handle<AccessorInfo> length =
      Accessors::FunctionLengthInfo(isolate(), attribs);
  {  // Add length.
    CallbacksDescriptor d(Handle<Name>(Name::cast(length->name())),
                          length, attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> name =
      Accessors::FunctionNameInfo(isolate(), attribs);
  {  // Add name.
    CallbacksDescriptor d(Handle<Name>(Name::cast(name->name())),
                          name, attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> args =
      Accessors::FunctionArgumentsInfo(isolate(), attribs);
  {  // Add arguments.
    CallbacksDescriptor d(Handle<Name>(Name::cast(args->name())),
                          args, attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> caller =
      Accessors::FunctionCallerInfo(isolate(), attribs);
  {  // Add caller.
    CallbacksDescriptor d(Handle<Name>(Name::cast(caller->name())),
                          caller, attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    if (function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE) {
      attribs = static_cast<PropertyAttributes>(attribs & ~READ_ONLY);
    }
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), attribs);
    CallbacksDescriptor d(Handle<Name>(Name::cast(prototype->name())),
                          prototype, attribs);
    map->AppendDescriptor(&d);
  }
}


Handle<Map> Genesis::CreateFunctionMap(FunctionMode function_mode) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetFunctionInstanceDescriptor(map, function_mode);
  map->set_function_with_prototype(IsFunctionModeWithPrototype(function_mode));
  return map;
}


Handle<JSFunction> Genesis::CreateEmptyFunction(Isolate* isolate) {
  // Allocate the map for function instances. Maps are allocated first and their
  // prototypes patched later, once empty function is created.

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      CreateFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  native_context()->set_sloppy_function_without_prototype_map(
      *function_without_prototype_map);

  // Allocate the function map. This map is temporary, used only for processing
  // of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> function_map =
      CreateFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE);
  native_context()->set_sloppy_function_map(*function_map);
  native_context()->set_sloppy_function_with_readonly_prototype_map(
      *function_map);

  // The final map for functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  sloppy_function_map_writable_prototype_ =
      CreateFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE);

  Factory* factory = isolate->factory();

  Handle<String> object_name = factory->Object_string();

  {  // --- O b j e c t ---
    Handle<JSFunction> object_fun = factory->NewFunction(object_name);
    Handle<Map> object_function_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    JSFunction::SetInitialMap(object_fun, object_function_map,
                              isolate->factory()->null_value());
    object_function_map->set_unused_property_fields(
        JSObject::kInitialGlobalObjectUnusedPropertiesCount);

    native_context()->set_object_function(*object_fun);

    // Allocate a new prototype for the object function.
    Handle<JSObject> prototype = factory->NewJSObject(
        isolate->object_function(),
        TENURED);
    Handle<Map> map = Map::Copy(handle(prototype->map()));
    map->set_is_prototype_map(true);
    prototype->set_map(*map);

    native_context()->set_initial_object_prototype(*prototype);
    // For bootstrapping set the array prototype to be the same as the object
    // prototype, otherwise the missing initial_array_prototype will cause
    // assertions during startup.
    native_context()->set_initial_array_prototype(*prototype);
    Accessors::FunctionSetPrototype(object_fun, prototype);
  }

  // Allocate the empty function as the prototype for function ECMAScript
  // 262 15.3.4.
  Handle<String> empty_string =
      factory->InternalizeOneByteString(STATIC_ASCII_VECTOR("Empty"));
  Handle<Code> code(isolate->builtins()->builtin(Builtins::kEmptyFunction));
  Handle<JSFunction> empty_function = factory->NewFunctionWithoutPrototype(
      empty_string, code);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_function_map =
      CreateFunctionMap(FUNCTION_WITHOUT_PROTOTYPE);
  DCHECK(!empty_function_map->is_dictionary_map());
  empty_function_map->set_prototype(
      native_context()->object_function()->prototype());
  empty_function_map->set_is_prototype_map(true);
  empty_function->set_map(*empty_function_map);

  // --- E m p t y ---
  Handle<String> source = factory->NewStringFromStaticAscii("() {}");
  Handle<Script> script = factory->NewScript(source);
  script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
  empty_function->shared()->set_script(*script);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
  empty_function->shared()->DontAdaptArguments();

  // Set prototypes for the function maps.
  native_context()->sloppy_function_map()->set_prototype(*empty_function);
  native_context()->sloppy_function_without_prototype_map()->
      set_prototype(*empty_function);
  sloppy_function_map_writable_prototype_->set_prototype(*empty_function);
  return empty_function;
}


void Genesis::SetStrictFunctionInstanceDescriptor(
    Handle<Map> map, FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 5 : 4;
  Map::EnsureDescriptorSlack(map, size);

  Handle<AccessorPair> arguments(factory()->NewAccessorPair());
  Handle<AccessorPair> caller(factory()->NewAccessorPair());
  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

  // Add length.
  if (function_mode == BOUND_FUNCTION) {
    Handle<String> length_string = isolate()->factory()->length_string();
    FieldDescriptor d(length_string, 0, ro_attribs, Representation::Tagged());
    map->AppendDescriptor(&d);
  } else {
    DCHECK(function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ||
           function_mode == FUNCTION_WITH_READONLY_PROTOTYPE ||
           function_mode == FUNCTION_WITHOUT_PROTOTYPE);
    Handle<AccessorInfo> length =
        Accessors::FunctionLengthInfo(isolate(), ro_attribs);
    CallbacksDescriptor d(Handle<Name>(Name::cast(length->name())),
                          length, ro_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> name =
      Accessors::FunctionNameInfo(isolate(), ro_attribs);
  {  // Add name.
    CallbacksDescriptor d(Handle<Name>(Name::cast(name->name())),
                          name, ro_attribs);
    map->AppendDescriptor(&d);
  }
  {  // Add arguments.
    CallbacksDescriptor d(factory()->arguments_string(), arguments,
                          rw_attribs);
    map->AppendDescriptor(&d);
  }
  {  // Add caller.
    CallbacksDescriptor d(factory()->caller_string(), caller, rw_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype.
    PropertyAttributes attribs =
        function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ? rw_attribs
                                                           : ro_attribs;
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), attribs);
    CallbacksDescriptor d(Handle<Name>(Name::cast(prototype->name())),
                          prototype, attribs);
    map->AppendDescriptor(&d);
  }
}


// ECMAScript 5th Edition, 13.2.3
Handle<JSFunction> Genesis::GetStrictPoisonFunction() {
  if (strict_poison_function.is_null()) {
    Handle<String> name = factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("ThrowTypeError"));
    Handle<Code> code(isolate()->builtins()->builtin(
        Builtins::kStrictModePoisonPill));
    strict_poison_function = factory()->NewFunctionWithoutPrototype(name, code);
    strict_poison_function->set_map(native_context()->sloppy_function_map());
    strict_poison_function->shared()->DontAdaptArguments();

    JSObject::PreventExtensions(strict_poison_function).Assert();
  }
  return strict_poison_function;
}


Handle<JSFunction> Genesis::GetGeneratorPoisonFunction() {
  if (generator_poison_function.is_null()) {
    Handle<String> name = factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("ThrowTypeError"));
    Handle<Code> code(isolate()->builtins()->builtin(
        Builtins::kGeneratorPoisonPill));
    generator_poison_function = factory()->NewFunctionWithoutPrototype(
        name, code);
    generator_poison_function->set_map(native_context()->sloppy_function_map());
    generator_poison_function->shared()->DontAdaptArguments();

    JSObject::PreventExtensions(generator_poison_function).Assert();
  }
  return generator_poison_function;
}


Handle<Map> Genesis::CreateStrictFunctionMap(
    FunctionMode function_mode,
    Handle<JSFunction> empty_function) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetStrictFunctionInstanceDescriptor(map, function_mode);
  map->set_function_with_prototype(IsFunctionModeWithPrototype(function_mode));
  map->set_prototype(*empty_function);
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
  // Special map for bound functions.
  Handle<Map> bound_function_map =
      CreateStrictFunctionMap(BOUND_FUNCTION, empty);
  native_context()->set_bound_function_map(*bound_function_map);

  // Complete the callbacks.
  PoisonArgumentsAndCaller(strict_function_without_prototype_map);
  PoisonArgumentsAndCaller(strict_function_map);
  PoisonArgumentsAndCaller(strict_function_map_writable_prototype_);
  PoisonArgumentsAndCaller(bound_function_map);
}


static void SetAccessors(Handle<Map> map,
                         Handle<String> name,
                         Handle<JSFunction> func) {
  DescriptorArray* descs = map->instance_descriptors();
  int number = descs->SearchWithCache(*name, *map);
  AccessorPair* accessors = AccessorPair::cast(descs->GetValue(number));
  accessors->set_getter(*func);
  accessors->set_setter(*func);
}


static void ReplaceAccessors(Handle<Map> map,
                             Handle<String> name,
                             PropertyAttributes attributes,
                             Handle<AccessorPair> accessor_pair) {
  DescriptorArray* descriptors = map->instance_descriptors();
  int idx = descriptors->SearchWithCache(*name, *map);
  CallbacksDescriptor descriptor(name, accessor_pair, attributes);
  descriptors->Replace(idx, &descriptor);
}


void Genesis::PoisonArgumentsAndCaller(Handle<Map> map) {
  SetAccessors(map, factory()->arguments_string(), GetStrictPoisonFunction());
  SetAccessors(map, factory()->caller_string(), GetStrictPoisonFunction());
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
  context->set(Context::NEXT_CONTEXT_LINK, heap->native_contexts_list());
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


Handle<JSGlobalProxy> Genesis::CreateNewGlobals(
    v8::Handle<v8::ObjectTemplate> global_proxy_template,
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    Handle<GlobalObject>* global_object_out) {
  // The argument global_proxy_template aka data is an ObjectTemplateInfo.
  // It has a constructor pointer that points at global_constructor which is a
  // FunctionTemplateInfo.
  // The global_proxy_constructor is used to create or reinitialize the
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
    Handle<Code> code = Handle<Code>(isolate()->builtins()->builtin(
        Builtins::kIllegal));
    Handle<JSObject> prototype =
        factory()->NewFunctionPrototype(isolate()->object_function());
    js_global_object_function = factory()->NewFunction(
        name, code, prototype, JS_GLOBAL_OBJECT_TYPE, JSGlobalObject::kSize);
#ifdef DEBUG
    LookupIterator it(prototype, factory()->constructor_string(),
                      LookupIterator::CHECK_OWN_REAL);
    Handle<Object> value = JSReceiver::GetProperty(&it).ToHandleChecked();
    DCHECK(it.IsFound());
    DCHECK_EQ(*isolate()->object_function(), *value);
#endif
  } else {
    Handle<FunctionTemplateInfo> js_global_object_constructor(
        FunctionTemplateInfo::cast(js_global_object_template->constructor()));
    js_global_object_function =
        factory()->CreateApiFunction(js_global_object_constructor,
                                     factory()->the_hole_value(),
                                     factory()->GlobalObjectType);
  }

  js_global_object_function->initial_map()->set_is_hidden_prototype();
  js_global_object_function->initial_map()->set_dictionary_map(true);
  Handle<GlobalObject> global_object =
      factory()->NewGlobalObject(js_global_object_function);
  if (global_object_out != NULL) {
    *global_object_out = global_object;
  }

  // Step 2: create or re-initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_proxy_template.IsEmpty()) {
    Handle<String> name = Handle<String>(heap()->empty_string());
    Handle<Code> code = Handle<Code>(isolate()->builtins()->builtin(
        Builtins::kIllegal));
    global_proxy_function = factory()->NewFunction(
        name, code, JS_GLOBAL_PROXY_TYPE, JSGlobalProxy::kSize);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor(
            FunctionTemplateInfo::cast(data->constructor()));
    global_proxy_function =
        factory()->CreateApiFunction(global_constructor,
                                     factory()->the_hole_value(),
                                     factory()->GlobalProxyType);
  }

  Handle<String> global_name = factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("global"));
  global_proxy_function->shared()->set_instance_class_name(*global_name);
  global_proxy_function->initial_map()->set_is_access_check_needed(true);

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  Handle<JSGlobalProxy> global_proxy;
  if (maybe_global_proxy.ToHandle(&global_proxy)) {
    factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  } else {
    global_proxy = Handle<JSGlobalProxy>::cast(
        factory()->NewJSObject(global_proxy_function, TENURED));
    global_proxy->set_hash(heap()->undefined_value());
  }
  return global_proxy;
}


void Genesis::HookUpGlobalProxy(Handle<GlobalObject> global_object,
                                Handle<JSGlobalProxy> global_proxy) {
  // Set the native context for the global object.
  global_object->set_native_context(*native_context());
  global_object->set_global_context(*native_context());
  global_object->set_global_proxy(*global_proxy);
  global_proxy->set_native_context(*native_context());
  native_context()->set_global_proxy(*global_proxy);
}


void Genesis::HookUpGlobalObject(Handle<GlobalObject> global_object) {
  Handle<GlobalObject> global_object_from_snapshot(
      GlobalObject::cast(native_context()->extension()));
  Handle<JSBuiltinsObject> builtins_global(native_context()->builtins());
  native_context()->set_extension(*global_object);
  native_context()->set_global_object(*global_object);
  native_context()->set_security_token(*global_object);
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  Runtime::DefineObjectProperty(builtins_global,
                                factory()->InternalizeOneByteString(
                                    STATIC_ASCII_VECTOR("global")),
                                global_object,
                                attributes).Assert();
  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(*global_object)->set_builtins(*builtins_global);
  TransferNamedProperties(global_object_from_snapshot, global_object);
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}


// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpGlobalObject.
void Genesis::InitializeGlobal(Handle<GlobalObject> global_object,
                               Handle<JSFunction> empty_function) {
  // --- N a t i v e   C o n t e x t ---
  // Use the empty function as closure (no scope info).
  native_context()->set_closure(*empty_function);
  native_context()->set_previous(NULL);
  // Set extension and global object.
  native_context()->set_extension(*global_object);
  native_context()->set_global_object(*global_object);
  // Security setup: Set the security token of the native context to the global
  // object. This makes the security check between two different contexts fail
  // by default even in case of global object reinitialization.
  native_context()->set_security_token(*global_object);

  Isolate* isolate = global_object->GetIsolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<String> object_name = factory->Object_string();
  JSObject::AddProperty(
      global_object, object_name, isolate->object_function(), DONT_ENUM);

  Handle<JSObject> global(native_context()->global_object());

  // Install global Function object
  InstallFunction(global, "Function", JS_FUNCTION_TYPE, JSFunction::kSize,
                  empty_function, Builtins::kIllegal);

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
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(array_length->name())),
          array_length, attribs);
      array_function->initial_map()->AppendDescriptor(&d);
    }

    // array_function is used internally. JS code creating array object should
    // search for the 'Array' property on the global object and use that one
    // as the constructor. 'Array' property on a global object can be
    // overwritten by JS code.
    native_context()->set_array_function(*array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(native_context(), initial_map);
    ArrayConstructorStub array_constructor_stub(isolate);
    Handle<Code> code = array_constructor_stub.GetCode();
    array_function->shared()->set_construct_stub(*code);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun =
        InstallFunction(global, "Number", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);
    native_context()->set_number_function(*number_fun);
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);
    native_context()->set_boolean_function(*boolean_fun);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun =
        InstallFunction(global, "String", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);
    string_fun->shared()->set_construct_stub(
        isolate->builtins()->builtin(Builtins::kStringConstructCode));
    native_context()->set_string_function(*string_fun);

    Handle<Map> string_map =
        Handle<Map>(native_context()->string_function()->initial_map());
    Map::EnsureDescriptorSlack(string_map, 1);

    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<AccessorInfo> string_length(
        Accessors::StringLengthInfo(isolate, attribs));

    {  // Add length.
      CallbacksDescriptor d(factory->length_string(), string_length, attribs);
      string_map->AppendDescriptor(&d);
    }
  }

  {
    // --- S y m b o l ---
    Handle<JSFunction> symbol_fun = InstallFunction(
        global, "Symbol", JS_VALUE_TYPE, JSValue::kSize,
        isolate->initial_object_prototype(), Builtins::kIllegal);
    native_context()->set_symbol_function(*symbol_fun);
  }

  {  // --- D a t e ---
    // Builtin functions for Date.prototype.
    Handle<JSFunction> date_fun =
        InstallFunction(global, "Date", JS_DATE_TYPE, JSDate::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);

    native_context()->set_date_function(*date_fun);
  }


  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal);
    native_context()->set_regexp_function(*regexp_fun);

    DCHECK(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map());

    DCHECK_EQ(0, initial_map->inobject_properties());

    PropertyAttributes final =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    Map::EnsureDescriptorSlack(initial_map, 5);

    {
      // ECMA-262, section 15.10.7.1.
      FieldDescriptor field(factory->source_string(),
                            JSRegExp::kSourceFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field);
    }
    {
      // ECMA-262, section 15.10.7.2.
      FieldDescriptor field(factory->global_string(),
                            JSRegExp::kGlobalFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field);
    }
    {
      // ECMA-262, section 15.10.7.3.
      FieldDescriptor field(factory->ignore_case_string(),
                            JSRegExp::kIgnoreCaseFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field);
    }
    {
      // ECMA-262, section 15.10.7.4.
      FieldDescriptor field(factory->multiline_string(),
                            JSRegExp::kMultilineFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field);
    }
    {
      // ECMA-262, section 15.10.7.5.
      PropertyAttributes writable =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
      FieldDescriptor field(factory->last_index_string(),
                            JSRegExp::kLastIndexFieldIndex,
                            writable,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field);
    }

    initial_map->set_inobject_properties(5);
    initial_map->set_pre_allocated_property_fields(5);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(
        initial_map->instance_size() + 5 * kPointerSize);
    initial_map->set_visitor_id(StaticVisitorBase::GetVisitorId(*initial_map));

    // RegExp prototype object is itself a RegExp.
    Handle<Map> proto_map = Map::Copy(initial_map);
    proto_map->set_prototype(native_context()->initial_object_prototype());
    Handle<JSObject> proto = factory->NewJSObjectFromMap(proto_map);
    proto->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex,
                                 heap->query_colon_string());
    proto->InObjectPropertyAtPut(JSRegExp::kGlobalFieldIndex,
                                 heap->false_value());
    proto->InObjectPropertyAtPut(JSRegExp::kIgnoreCaseFieldIndex,
                                 heap->false_value());
    proto->InObjectPropertyAtPut(JSRegExp::kMultilineFieldIndex,
                                 heap->false_value());
    proto->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex,
                                 Smi::FromInt(0),
                                 SKIP_WRITE_BARRIER);  // It's a Smi.
    proto_map->set_is_prototype_map(true);
    initial_map->set_prototype(*proto);
    factory->SetRegExpIrregexpData(Handle<JSRegExp>::cast(proto),
                                   JSRegExp::IRREGEXP, factory->empty_string(),
                                   JSRegExp::Flags(0), 0);
  }

  {  // -- J S O N
    Handle<String> name = factory->InternalizeUtf8String("JSON");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetInstancePrototype(cons,
        Handle<Object>(native_context()->initial_object_prototype(), isolate));
    cons->SetInstanceClassName(*name);
    Handle<JSObject> json_object = factory->NewJSObject(cons, TENURED);
    DCHECK(json_object->IsJSObject());
    JSObject::AddProperty(global, name, json_object, DONT_ENUM);
    native_context()->set_json_object(*json_object);
  }

  {  // -- A r r a y B u f f e r
    Handle<JSFunction> array_buffer_fun =
        InstallFunction(
            global, "ArrayBuffer", JS_ARRAY_BUFFER_TYPE,
            JSArrayBuffer::kSizeWithInternalFields,
            isolate->initial_object_prototype(),
            Builtins::kIllegal);
    native_context()->set_array_buffer_fun(*array_buffer_fun);
  }

  {  // -- T y p e d A r r a y s
#define INSTALL_TYPED_ARRAY(Type, type, TYPE, ctype, size)                    \
    {                                                                         \
      Handle<JSFunction> fun;                                                 \
      Handle<Map> external_map;                                               \
      InstallTypedArray(#Type "Array",                                        \
          TYPE##_ELEMENTS,                                                    \
          &fun,                                                               \
          &external_map);                                                     \
      native_context()->set_##type##_array_fun(*fun);                         \
      native_context()->set_##type##_array_external_map(*external_map);       \
    }
    TYPED_ARRAYS(INSTALL_TYPED_ARRAY)
#undef INSTALL_TYPED_ARRAY

    Handle<JSFunction> data_view_fun =
        InstallFunction(
            global, "DataView", JS_DATA_VIEW_TYPE,
            JSDataView::kSizeWithInternalFields,
            isolate->initial_object_prototype(),
            Builtins::kIllegal);
    native_context()->set_data_view_fun(*data_view_fun);
  }

  // -- M a p
  InstallFunction(global, "Map", JS_MAP_TYPE, JSMap::kSize,
                  isolate->initial_object_prototype(), Builtins::kIllegal);

  // -- S e t
  InstallFunction(global, "Set", JS_SET_TYPE, JSSet::kSize,
                  isolate->initial_object_prototype(), Builtins::kIllegal);

  {  // Set up the iterator result object
    STATIC_ASSERT(JSGeneratorObject::kResultPropertyCount == 2);
    Handle<JSFunction> object_function(native_context()->object_function());
    DCHECK(object_function->initial_map()->inobject_properties() == 0);
    Handle<Map> iterator_result_map =
        Map::Create(object_function, JSGeneratorObject::kResultPropertyCount);
    DCHECK(iterator_result_map->inobject_properties() ==
           JSGeneratorObject::kResultPropertyCount);
    Map::EnsureDescriptorSlack(iterator_result_map,
                               JSGeneratorObject::kResultPropertyCount);

    FieldDescriptor value_descr(factory->value_string(),
                                JSGeneratorObject::kResultValuePropertyIndex,
                                NONE, Representation::Tagged());
    iterator_result_map->AppendDescriptor(&value_descr);

    FieldDescriptor done_descr(factory->done_string(),
                               JSGeneratorObject::kResultDonePropertyIndex,
                               NONE, Representation::Tagged());
    iterator_result_map->AppendDescriptor(&done_descr);

    iterator_result_map->set_unused_property_fields(0);
    DCHECK_EQ(JSGeneratorObject::kResultSize,
              iterator_result_map->instance_size());
    native_context()->set_iterator_result_map(*iterator_result_map);
  }

  // -- W e a k M a p
  InstallFunction(global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kSize,
                  isolate->initial_object_prototype(), Builtins::kIllegal);
  // -- W e a k S e t
  InstallFunction(global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kSize,
                  isolate->initial_object_prototype(), Builtins::kIllegal);

  {  // --- sloppy arguments map
    // Make sure we can recognize argument objects at runtime.
    // This is done by introducing an anonymous function with
    // class_name equals 'Arguments'.
    Handle<String> arguments_string = factory->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("Arguments"));
    Handle<Code> code(isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSFunction> function = factory->NewFunctionWithoutPrototype(
        arguments_string, code);
    function->shared()->set_instance_class_name(*arguments_string);

    Handle<Map> map =
        factory->NewMap(JS_OBJECT_TYPE, Heap::kSloppyArgumentsObjectSize);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 2);

    {  // length
      FieldDescriptor d(factory->length_string(), Heap::kArgumentsLengthIndex,
                        DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      FieldDescriptor d(factory->callee_string(), Heap::kArgumentsCalleeIndex,
                        DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    map->set_function_with_prototype(true);
    map->set_pre_allocated_property_fields(2);
    map->set_inobject_properties(2);
    native_context()->set_sloppy_arguments_map(*map);

    DCHECK(!function->has_initial_map());
    JSFunction::SetInitialMap(function, map,
                              isolate->initial_object_prototype());

    DCHECK(map->inobject_properties() > Heap::kArgumentsCalleeIndex);
    DCHECK(map->inobject_properties() > Heap::kArgumentsLengthIndex);
    DCHECK(!map->is_dictionary_map());
    DCHECK(IsFastObjectElementsKind(map->elements_kind()));
  }

  {  // --- aliased arguments map
    Handle<Map> map = Map::Copy(isolate->sloppy_arguments_map());
    map->set_elements_kind(SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->pre_allocated_property_fields());
    native_context()->set_aliased_arguments_map(*map);
  }

  {  // --- strict mode arguments map
    const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError functions.
    Handle<AccessorPair> callee = factory->NewAccessorPair();
    Handle<AccessorPair> caller = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetStrictPoisonFunction();

    // Install the ThrowTypeError functions.
    callee->set_getter(*poison);
    callee->set_setter(*poison);
    caller->set_getter(*poison);
    caller->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE,
                                      Heap::kStrictArgumentsObjectSize);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(map, 3);

    {  // length
      FieldDescriptor d(factory->length_string(), Heap::kArgumentsLengthIndex,
                        DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      CallbacksDescriptor d(factory->callee_string(), callee, attributes);
      map->AppendDescriptor(&d);
    }
    {  // caller
      CallbacksDescriptor d(factory->caller_string(), caller, attributes);
      map->AppendDescriptor(&d);
    }

    map->set_function_with_prototype(true);
    map->set_prototype(native_context()->object_function()->prototype());
    map->set_pre_allocated_property_fields(1);
    map->set_inobject_properties(1);

    // Copy constructor from the sloppy arguments boilerplate.
    map->set_constructor(
        native_context()->sloppy_arguments_map()->constructor());

    native_context()->set_strict_arguments_map(*map);

    DCHECK(map->inobject_properties() > Heap::kArgumentsLengthIndex);
    DCHECK(!map->is_dictionary_map());
    DCHECK(IsFastObjectElementsKind(map->elements_kind()));
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<Code> code = Handle<Code>(
        isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSFunction> context_extension_fun = factory->NewFunction(
        factory->empty_string(), code, JS_CONTEXT_EXTENSION_OBJECT_TYPE,
        JSObject::kHeaderSize);

    Handle<String> name = factory->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("context_extension"));
    context_extension_fun->shared()->set_instance_class_name(*name);
    native_context()->set_context_extension_function(*context_extension_fun);
  }


  {
    // Set up the call-as-function delegate.
    Handle<Code> code =
        Handle<Code>(isolate->builtins()->builtin(
            Builtins::kHandleApiCallAsFunction));
    Handle<JSFunction> delegate = factory->NewFunction(
        factory->empty_string(), code, JS_OBJECT_TYPE, JSObject::kHeaderSize);
    native_context()->set_call_as_function_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<Code> code =
        Handle<Code>(isolate->builtins()->builtin(
            Builtins::kHandleApiCallAsConstructor));
    Handle<JSFunction> delegate = factory->NewFunction(
        factory->empty_string(), code, JS_OBJECT_TYPE, JSObject::kHeaderSize);
    native_context()->set_call_as_constructor_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  // Initialize the embedder data slot.
  Handle<FixedArray> embedder_data = factory->NewFixedArray(3);
  native_context()->set_embedder_data(*embedder_data);
}


void Genesis::InstallTypedArray(
    const char* name,
    ElementsKind elements_kind,
    Handle<JSFunction>* fun,
    Handle<Map>* external_map) {
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

  ElementsKind external_kind = GetNextTransitionElementsKind(elements_kind);
  *external_map = Map::AsElementsKind(initial_map, external_kind);
}


void Genesis::InitializeExperimentalGlobal() {
  // TODO(mstarzinger): Move this into Genesis::InitializeGlobal once we no
  // longer need to live behind flags, so functions get added to the snapshot.

  if (FLAG_harmony_generators) {
    // Create generator meta-objects and install them on the builtins object.
    Handle<JSObject> builtins(native_context()->builtins());
    Handle<JSObject> generator_object_prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Handle<JSFunction> generator_function_prototype = InstallFunction(
        builtins, "GeneratorFunctionPrototype", JS_FUNCTION_TYPE,
        JSFunction::kHeaderSize, generator_object_prototype,
        Builtins::kIllegal);
    InstallFunction(builtins, "GeneratorFunction",
                    JS_FUNCTION_TYPE, JSFunction::kSize,
                    generator_function_prototype, Builtins::kIllegal);

    // Create maps for generator functions and their prototypes.  Store those
    // maps in the native context.
    Handle<Map> sloppy_function_map(native_context()->sloppy_function_map());
    Handle<Map> generator_function_map = Map::Copy(sloppy_function_map);
    generator_function_map->set_prototype(*generator_function_prototype);
    native_context()->set_sloppy_generator_function_map(
        *generator_function_map);

    // The "arguments" and "caller" instance properties aren't specified, so
    // technically we could leave them out.  They make even less sense for
    // generators than for functions.  Still, the same argument that it makes
    // sense to keep them around but poisoned in strict mode applies to
    // generators as well.  With poisoned accessors, naive callers can still
    // iterate over the properties without accessing them.
    //
    // We can't use PoisonArgumentsAndCaller because that mutates accessor pairs
    // in place, and the initial state of the generator function map shares the
    // accessor pair with sloppy functions.  Also the error message should be
    // different.  Also unhappily, we can't use the API accessors to implement
    // poisoning, because API accessors present themselves as data properties,
    // not accessor properties, and so getOwnPropertyDescriptor raises an
    // exception as it tries to get the values.  Sadness.
    Handle<AccessorPair> poison_pair(factory()->NewAccessorPair());
    PropertyAttributes rw_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    Handle<JSFunction> poison_function = GetGeneratorPoisonFunction();
    poison_pair->set_getter(*poison_function);
    poison_pair->set_setter(*poison_function);
    ReplaceAccessors(generator_function_map, factory()->arguments_string(),
        rw_attribs, poison_pair);
    ReplaceAccessors(generator_function_map, factory()->caller_string(),
        rw_attribs, poison_pair);

    Handle<Map> strict_function_map(native_context()->strict_function_map());
    Handle<Map> strict_generator_function_map = Map::Copy(strict_function_map);
    // "arguments" and "caller" already poisoned.
    strict_generator_function_map->set_prototype(*generator_function_prototype);
    native_context()->set_strict_generator_function_map(
        *strict_generator_function_map);

    Handle<JSFunction> object_function(native_context()->object_function());
    Handle<Map> generator_object_prototype_map = Map::Create(
        object_function, 0);
    generator_object_prototype_map->set_prototype(
        *generator_object_prototype);
    native_context()->set_generator_object_prototype_map(
        *generator_object_prototype_map);
  }
}


bool Genesis::CompileBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->NativesSourceLookup(index);
  return CompileNative(isolate, name, source_code);
}


bool Genesis::CompileExperimentalBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = ExperimentalNatives::GetScriptName(index);
  Factory* factory = isolate->factory();
  Handle<String> source_code;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, source_code,
      factory->NewStringFromAscii(
          ExperimentalNatives::GetRawScriptSource(index)),
      false);
  return CompileNative(isolate, name, source_code);
}


bool Genesis::CompileNative(Isolate* isolate,
                            Vector<const char> name,
                            Handle<String> source) {
  HandleScope scope(isolate);
  SuppressDebug compiling_natives(isolate->debug());
  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) return false;

  bool result = CompileScriptCached(isolate,
                                    name,
                                    source,
                                    NULL,
                                    NULL,
                                    Handle<Context>(isolate->context()),
                                    true);
  DCHECK(isolate->has_pending_exception() != result);
  if (!result) isolate->clear_pending_exception();
  return result;
}


bool Genesis::CompileScriptCached(Isolate* isolate,
                                  Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> function_info;

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  if (cache == NULL || !cache->Lookup(name, &function_info)) {
    DCHECK(source->IsOneByteRepresentation());
    Handle<String> script_name =
        factory->NewStringFromUtf8(name).ToHandleChecked();
    function_info = Compiler::CompileScript(
        source, script_name, 0, 0, false, top_context, extension, NULL,
        ScriptCompiler::kNoCompileOptions,
        use_runtime_context ? NATIVES_CODE : NOT_NATIVES_CODE);
    if (function_info.is_null()) return false;
    if (cache != NULL) cache->Add(name, function_info);
  }

  // Set up the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  DCHECK(top_context->IsNativeContext());
  Handle<Context> context =
      Handle<Context>(use_runtime_context
                      ? Handle<Context>(top_context->runtime_context())
                      : top_context);
  Handle<JSFunction> fun =
      factory->NewFunctionFromSharedFunctionInfo(function_info, context);

  // Call function using either the runtime object or the global
  // object as the receiver. Provide no parameters.
  Handle<Object> receiver =
      Handle<Object>(use_runtime_context
                     ? top_context->builtins()
                     : top_context->global_object(),
                     isolate);
  return !Execution::Call(
      isolate, fun, receiver, 0, NULL).is_null();
}


static Handle<JSObject> ResolveBuiltinIdHolder(Handle<Context> native_context,
                                               const char* holder_expr) {
  Isolate* isolate = native_context->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<GlobalObject> global(native_context->global_object());
  const char* period_pos = strchr(holder_expr, '.');
  if (period_pos == NULL) {
    return Handle<JSObject>::cast(
        Object::GetPropertyOrElement(
            global, factory->InternalizeUtf8String(holder_expr))
            .ToHandleChecked());
  }
  const char* inner = period_pos + 1;
  DCHECK_EQ(NULL, strchr(inner, '.'));
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


#define INSTALL_NATIVE(Type, name, var)                                        \
  Handle<String> var##_name =                                                  \
      factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR(name));          \
  Handle<Object> var##_native = Object::GetProperty(                           \
      handle(native_context()->builtins()), var##_name).ToHandleChecked();     \
  native_context()->set_##var(Type::cast(*var##_native));

#define INSTALL_NATIVE_MATH(name)                                    \
  {                                                                  \
    Handle<Object> fun =                                             \
        ResolveBuiltinIdHolder(native_context(), "Math." #name);     \
    native_context()->set_math_##name##_fun(JSFunction::cast(*fun)); \
  }

void Genesis::InstallNativeFunctions() {
  HandleScope scope(isolate());
  INSTALL_NATIVE(JSFunction, "CreateDate", create_date_fun);

  INSTALL_NATIVE(JSFunction, "ToNumber", to_number_fun);
  INSTALL_NATIVE(JSFunction, "ToString", to_string_fun);
  INSTALL_NATIVE(JSFunction, "ToDetailString", to_detail_string_fun);
  INSTALL_NATIVE(JSFunction, "ToObject", to_object_fun);
  INSTALL_NATIVE(JSFunction, "ToInteger", to_integer_fun);
  INSTALL_NATIVE(JSFunction, "ToUint32", to_uint32_fun);
  INSTALL_NATIVE(JSFunction, "ToInt32", to_int32_fun);

  INSTALL_NATIVE(JSFunction, "GlobalEval", global_eval_fun);
  INSTALL_NATIVE(JSFunction, "Instantiate", instantiate_fun);
  INSTALL_NATIVE(JSFunction, "ConfigureTemplateInstance",
                 configure_instance_fun);
  INSTALL_NATIVE(JSFunction, "GetStackTraceLine", get_stack_trace_line_fun);
  INSTALL_NATIVE(JSObject, "functionCache", function_cache);
  INSTALL_NATIVE(JSFunction, "ToCompletePropertyDescriptor",
                 to_complete_property_descriptor);

  INSTALL_NATIVE(JSFunction, "IsPromise", is_promise);
  INSTALL_NATIVE(JSFunction, "PromiseCreate", promise_create);
  INSTALL_NATIVE(JSFunction, "PromiseResolve", promise_resolve);
  INSTALL_NATIVE(JSFunction, "PromiseReject", promise_reject);
  INSTALL_NATIVE(JSFunction, "PromiseChain", promise_chain);
  INSTALL_NATIVE(JSFunction, "PromiseCatch", promise_catch);
  INSTALL_NATIVE(JSFunction, "PromiseThen", promise_then);

  INSTALL_NATIVE(JSFunction, "NotifyChange", observers_notify_change);
  INSTALL_NATIVE(JSFunction, "EnqueueSpliceRecord", observers_enqueue_splice);
  INSTALL_NATIVE(JSFunction, "BeginPerformSplice",
                 observers_begin_perform_splice);
  INSTALL_NATIVE(JSFunction, "EndPerformSplice",
                 observers_end_perform_splice);
  INSTALL_NATIVE(JSFunction, "NativeObjectObserve",
                 native_object_observe);
  INSTALL_NATIVE(JSFunction, "NativeObjectGetNotifier",
                 native_object_get_notifier);
  INSTALL_NATIVE(JSFunction, "NativeObjectNotifierPerformChange",
                 native_object_notifier_perform_change);

  INSTALL_NATIVE(Symbol, "symbolIterator", iterator_symbol);
  INSTALL_NATIVE(Symbol, "symbolUnscopables", unscopables_symbol);

  INSTALL_NATIVE_MATH(abs)
  INSTALL_NATIVE_MATH(acos)
  INSTALL_NATIVE_MATH(asin)
  INSTALL_NATIVE_MATH(atan)
  INSTALL_NATIVE_MATH(atan2)
  INSTALL_NATIVE_MATH(ceil)
  INSTALL_NATIVE_MATH(cos)
  INSTALL_NATIVE_MATH(exp)
  INSTALL_NATIVE_MATH(floor)
  INSTALL_NATIVE_MATH(imul)
  INSTALL_NATIVE_MATH(log)
  INSTALL_NATIVE_MATH(max)
  INSTALL_NATIVE_MATH(min)
  INSTALL_NATIVE_MATH(pow)
  INSTALL_NATIVE_MATH(random)
  INSTALL_NATIVE_MATH(round)
  INSTALL_NATIVE_MATH(sin)
  INSTALL_NATIVE_MATH(sqrt)
  INSTALL_NATIVE_MATH(tan)
}


void Genesis::InstallExperimentalNativeFunctions() {
  if (FLAG_harmony_proxies) {
    INSTALL_NATIVE(JSFunction, "DerivedHasTrap", derived_has_trap);
    INSTALL_NATIVE(JSFunction, "DerivedGetTrap", derived_get_trap);
    INSTALL_NATIVE(JSFunction, "DerivedSetTrap", derived_set_trap);
    INSTALL_NATIVE(JSFunction, "ProxyEnumerate", proxy_enumerate);
  }
}

#undef INSTALL_NATIVE


Handle<JSFunction> Genesis::InstallInternalArray(
    Handle<JSBuiltinsObject> builtins,
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
  Handle<JSFunction> array_function = InstallFunction(
      builtins, name, JS_ARRAY_TYPE, JSArray::kSize,
      prototype, Builtins::kInternalArrayCode);

  InternalArrayConstructorStub internal_array_constructor_stub(isolate());
  Handle<Code> code = internal_array_constructor_stub.GetCode();
  array_function->shared()->set_construct_stub(*code);
  array_function->shared()->DontAdaptArguments();

  Handle<Map> original_map(array_function->initial_map());
  Handle<Map> initial_map = Map::Copy(original_map);
  initial_map->set_elements_kind(elements_kind);
  JSFunction::SetInitialMap(array_function, initial_map, prototype);

  // Make "length" magic on instances.
  Map::EnsureDescriptorSlack(initial_map, 1);

  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE);

  Handle<AccessorInfo> array_length =
      Accessors::ArrayLengthInfo(isolate(), attribs);
  {  // Add length.
    CallbacksDescriptor d(
        Handle<Name>(Name::cast(array_length->name())), array_length, attribs);
    array_function->initial_map()->AppendDescriptor(&d);
  }

  return array_function;
}


bool Genesis::InstallNatives() {
  HandleScope scope(isolate());

  // Create a function for the builtins object. Allocate space for the
  // JavaScript builtins, a reference to the builtins object
  // (itself) and a reference to the native_context directly in the object.
  Handle<Code> code = Handle<Code>(
      isolate()->builtins()->builtin(Builtins::kIllegal));
  Handle<JSFunction> builtins_fun = factory()->NewFunction(
      factory()->empty_string(), code, JS_BUILTINS_OBJECT_TYPE,
      JSBuiltinsObject::kSize);

  Handle<String> name =
      factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR("builtins"));
  builtins_fun->shared()->set_instance_class_name(*name);
  builtins_fun->initial_map()->set_dictionary_map(true);
  builtins_fun->initial_map()->set_prototype(heap()->null_value());

  // Allocate the builtins object.
  Handle<JSBuiltinsObject> builtins =
      Handle<JSBuiltinsObject>::cast(factory()->NewGlobalObject(builtins_fun));
  builtins->set_builtins(*builtins);
  builtins->set_native_context(*native_context());
  builtins->set_global_context(*native_context());
  builtins->set_global_proxy(native_context()->global_proxy());


  // Set up the 'global' properties of the builtins object. The
  // 'global' property that refers to the global object is the only
  // way to get from code running in the builtins context to the
  // global object.
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  Handle<String> global_string =
      factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR("global"));
  Handle<Object> global_obj(native_context()->global_object(), isolate());
  JSObject::AddProperty(builtins, global_string, global_obj, attributes);
  Handle<String> builtins_string =
      factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR("builtins"));
  JSObject::AddProperty(builtins, builtins_string, builtins, attributes);

  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(native_context()->global_object())->
      set_builtins(*builtins);

  // Create a bridge function that has context in the native context.
  Handle<JSFunction> bridge = factory()->NewFunction(factory()->empty_string());
  DCHECK(bridge->context() == *isolate()->native_context());

  // Allocate the builtins context.
  Handle<Context> context =
    factory()->NewFunctionContext(Context::MIN_CONTEXT_SLOTS, bridge);
  context->set_global_object(*builtins);  // override builtins global object

  native_context()->set_runtime_context(*context);

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun = InstallFunction(
        builtins, "Script", JS_VALUE_TYPE, JSValue::kSize,
        isolate()->initial_object_prototype(), Builtins::kIllegal);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Accessors::FunctionSetPrototype(script_fun, prototype);
    native_context()->set_script_function(*script_fun);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());
    Map::EnsureDescriptorSlack(script_map, 14);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    Handle<AccessorInfo> script_column =
        Accessors::ScriptColumnOffsetInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_column->name())),
                           script_column, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_id =
        Accessors::ScriptIdInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_id->name())),
                            script_id, attribs);
      script_map->AppendDescriptor(&d);
    }


    Handle<AccessorInfo> script_name =
        Accessors::ScriptNameInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_name->name())),
                            script_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_line =
        Accessors::ScriptLineOffsetInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_line->name())),
                           script_line, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source =
        Accessors::ScriptSourceInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_source->name())),
                            script_source, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_type =
        Accessors::ScriptTypeInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_type->name())),
                            script_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_compilation_type =
        Accessors::ScriptCompilationTypeInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_compilation_type->name())),
          script_compilation_type, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_line_ends =
        Accessors::ScriptLineEndsInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_line_ends->name())),
                            script_line_ends, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_context_data =
        Accessors::ScriptContextDataInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_context_data->name())),
          script_context_data, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script =
        Accessors::ScriptEvalFromScriptInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_script->name())),
          script_eval_from_script, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_script_position =
        Accessors::ScriptEvalFromScriptPositionInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_script_position->name())),
          script_eval_from_script_position, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_eval_from_function_name =
        Accessors::ScriptEvalFromFunctionNameInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_eval_from_function_name->name())),
          script_eval_from_function_name, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_url =
        Accessors::ScriptSourceUrlInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(Handle<Name>(Name::cast(script_source_url->name())),
                            script_source_url, attribs);
      script_map->AppendDescriptor(&d);
    }

    Handle<AccessorInfo> script_source_mapping_url =
        Accessors::ScriptSourceMappingUrlInfo(isolate(), attribs);
    {
      CallbacksDescriptor d(
          Handle<Name>(Name::cast(script_source_mapping_url->name())),
          script_source_mapping_url, attribs);
      script_map->AppendDescriptor(&d);
    }

    // Allocate the empty script.
    Handle<Script> script = factory()->NewScript(factory()->empty_string());
    script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
    heap()->public_set_empty_script(*script);
  }
  {
    // Builtin function for OpaqueReference -- a JSValue-based object,
    // that keeps its field isolated from JavaScript code. It may store
    // objects, that JavaScript code may not access.
    Handle<JSFunction> opaque_reference_fun = InstallFunction(
        builtins, "OpaqueReference", JS_VALUE_TYPE, JSValue::kSize,
        isolate()->initial_object_prototype(), Builtins::kIllegal);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Accessors::FunctionSetPrototype(opaque_reference_fun, prototype);
    native_context()->set_opaque_reference_function(*opaque_reference_fun);
  }

  // InternalArrays should not use Smi-Only array optimizations. There are too
  // many places in the C++ runtime code (e.g. RegEx) that assume that
  // elements in InternalArrays can be set to non-Smi values without going
  // through a common bottleneck that would make the SMI_ONLY -> FAST_ELEMENT
  // transition easy to trap. Moreover, they rarely are smi-only.
  {
    Handle<JSFunction> array_function =
        InstallInternalArray(builtins, "InternalArray", FAST_HOLEY_ELEMENTS);
    native_context()->set_internal_array_function(*array_function);
  }

  {
    InstallInternalArray(builtins, "InternalPackedArray", FAST_ELEMENTS);
  }

  {  // -- S e t I t e r a t o r
    Handle<JSFunction> set_iterator_function = InstallFunction(
        builtins, "SetIterator", JS_SET_ITERATOR_TYPE, JSSetIterator::kSize,
        isolate()->initial_object_prototype(), Builtins::kIllegal);
    native_context()->set_set_iterator_map(
        set_iterator_function->initial_map());
  }

  {  // -- M a p I t e r a t o r
    Handle<JSFunction> map_iterator_function = InstallFunction(
        builtins, "MapIterator", JS_MAP_ITERATOR_TYPE, JSMapIterator::kSize,
        isolate()->initial_object_prototype(), Builtins::kIllegal);
    native_context()->set_map_iterator_map(
        map_iterator_function->initial_map());
  }

  if (FLAG_disable_native_files) {
    PrintF("Warning: Running without installed natives!\n");
    return true;
  }

  // Install natives.
  for (int i = Natives::GetDebuggerCount();
       i < Natives::GetBuiltinsCount();
       i++) {
    if (!CompileBuiltin(isolate(), i)) return false;
    // TODO(ager): We really only need to install the JS builtin
    // functions on the builtins object after compiling and running
    // runtime.js.
    if (!InstallJSBuiltins(builtins)) return false;
  }

  InstallNativeFunctions();

  // Store the map for the string prototype after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function());
  DCHECK(JSObject::cast(
      string_function->initial_map()->prototype())->HasFastProperties());
  native_context()->set_string_function_prototype_map(
      HeapObject::cast(string_function->initial_map()->prototype())->map());

  // Install Function.prototype.call and apply.
  { Handle<String> key = factory()->function_class_string();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(Object::GetProperty(
            handle(native_context()->global_object()), key).ToHandleChecked());
    Handle<JSObject> proto =
        Handle<JSObject>(JSObject::cast(function->instance_prototype()));

    // Install the call and the apply functions.
    Handle<JSFunction> call =
        InstallFunction(proto, "call", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        MaybeHandle<JSObject>(), Builtins::kFunctionCall);
    Handle<JSFunction> apply =
        InstallFunction(proto, "apply", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        MaybeHandle<JSObject>(), Builtins::kFunctionApply);
    if (FLAG_vector_ics) {
      // Apply embeds an IC, so we need a type vector of size 1 in the shared
      // function info.
      Handle<FixedArray> feedback_vector = factory()->NewTypeFeedbackVector(1);
      apply->shared()->set_feedback_vector(*feedback_vector);
    }

    // Make sure that Function.prototype.call appears to be compiled.
    // The code will never be called, but inline caching for call will
    // only work if it appears to be compiled.
    call->shared()->DontAdaptArguments();
    DCHECK(call->is_compiled());

    // Set the expected parameters for apply to 2; required by builtin.
    apply->shared()->set_formal_parameter_count(2);

    // Set the lengths for the functions to satisfy ECMA-262.
    call->shared()->set_length(1);
    apply->shared()->set_length(2);
  }

  InstallBuiltinFunctionIds();

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
    initial_map->set_constructor(*array_constructor);

    // Set prototype on map.
    initial_map->set_non_instance_prototype(false);
    initial_map->set_prototype(*array_prototype);

    // Update map with length accessor from Array and add "index" and "input".
    Map::EnsureDescriptorSlack(initial_map, 3);

    {
      JSFunction* array_function = native_context()->array_function();
      Handle<DescriptorArray> array_descriptors(
          array_function->initial_map()->instance_descriptors());
      Handle<String> length = factory()->length_string();
      int old = array_descriptors->SearchWithCache(
          *length, array_function->initial_map());
      DCHECK(old != DescriptorArray::kNotFound);
      CallbacksDescriptor desc(length,
                               handle(array_descriptors->GetValue(old),
                                      isolate()),
                               array_descriptors->GetDetails(old).attributes());
      initial_map->AppendDescriptor(&desc);
    }
    {
      FieldDescriptor index_field(factory()->index_string(),
                                  JSRegExpResult::kIndexIndex,
                                  NONE,
                                  Representation::Tagged());
      initial_map->AppendDescriptor(&index_field);
    }

    {
      FieldDescriptor input_field(factory()->input_string(),
                                  JSRegExpResult::kInputIndex,
                                  NONE,
                                  Representation::Tagged());
      initial_map->AppendDescriptor(&input_field);
    }

    initial_map->set_inobject_properties(2);
    initial_map->set_pre_allocated_property_fields(2);
    initial_map->set_unused_property_fields(0);

    native_context()->set_regexp_result_map(*initial_map);
  }

#ifdef VERIFY_HEAP
  builtins->ObjectVerify();
#endif

  return true;
}


#define INSTALL_EXPERIMENTAL_NATIVE(i, flag, file)                \
  if (FLAG_harmony_##flag &&                                      \
      strcmp(ExperimentalNatives::GetScriptName(i).start(),       \
          "native " file) == 0) {                                 \
    if (!CompileExperimentalBuiltin(isolate(), i)) return false;  \
  }


bool Genesis::InstallExperimentalNatives() {
  for (int i = ExperimentalNatives::GetDebuggerCount();
       i < ExperimentalNatives::GetBuiltinsCount();
       i++) {
    INSTALL_EXPERIMENTAL_NATIVE(i, proxies, "proxy.js")
    INSTALL_EXPERIMENTAL_NATIVE(i, generators, "generator.js")
    INSTALL_EXPERIMENTAL_NATIVE(i, strings, "harmony-string.js")
    INSTALL_EXPERIMENTAL_NATIVE(i, arrays, "harmony-array.js")
  }

  InstallExperimentalNativeFunctions();
  return true;
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


void Genesis::InstallBuiltinFunctionIds() {
  HandleScope scope(isolate());
#define INSTALL_BUILTIN_ID(holder_expr, fun_name, name) \
  {                                                     \
    Handle<JSObject> holder = ResolveBuiltinIdHolder(   \
        native_context(), #holder_expr);                \
    BuiltinFunctionId id = k##name;                     \
    InstallBuiltinFunctionId(holder, #fun_name, id);    \
  }
  FUNCTIONS_WITH_ID_LIST(INSTALL_BUILTIN_ID)
#undef INSTALL_BUILTIN_ID
}


// Do not forget to update macros.py with named constant
// of cache id.
#define JSFUNCTION_RESULT_CACHE_LIST(F) \
  F(16, native_context()->regexp_function())


static FixedArray* CreateCache(int size, Handle<JSFunction> factory_function) {
  Factory* factory = factory_function->GetIsolate()->factory();
  // Caches are supposed to live for a long time, allocate in old space.
  int array_size = JSFunctionResultCache::kEntriesIndex + 2 * size;
  // Cannot use cast as object is not fully initialized yet.
  JSFunctionResultCache* cache = reinterpret_cast<JSFunctionResultCache*>(
      *factory->NewFixedArrayWithHoles(array_size, TENURED));
  cache->set(JSFunctionResultCache::kFactoryIndex, *factory_function);
  cache->MakeZeroSize();
  return cache;
}


void Genesis::InstallJSFunctionResultCaches() {
  const int kNumberOfCaches = 0 +
#define F(size, func) + 1
    JSFUNCTION_RESULT_CACHE_LIST(F)
#undef F
  ;

  Handle<FixedArray> caches =
      factory()->NewFixedArray(kNumberOfCaches, TENURED);

  int index = 0;

#define F(size, func) do {                                              \
    FixedArray* cache = CreateCache((size), Handle<JSFunction>(func));  \
    caches->set(index++, cache);                                        \
  } while (false)

  JSFUNCTION_RESULT_CACHE_LIST(F);

#undef F

  native_context()->set_jsfunction_result_caches(*caches);
}


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

  Handle<JSObject> Error = Handle<JSObject>::cast(
      Object::GetProperty(isolate, global, "Error").ToHandleChecked());
  Handle<String> name =
      factory->InternalizeOneByteString(STATIC_ASCII_VECTOR("stackTraceLimit"));
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(Error, name, stack_trace_limit, NONE);

  // Expose the natives in global if a name for it is specified.
  if (FLAG_expose_natives_as != NULL && strlen(FLAG_expose_natives_as) != 0) {
    Handle<String> natives =
        factory->InternalizeUtf8String(FLAG_expose_natives_as);
    JSObject::AddProperty(global, natives, handle(global->builtins()),
                          DONT_ENUM);
  }

  // Expose the stack trace symbol to native JS.
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      JSObject::SetOwnPropertyIgnoreAttributes(
          handle(native_context->builtins(), isolate),
          factory->InternalizeOneByteString(
              STATIC_ASCII_VECTOR("stack_trace_symbol")),
          factory->stack_trace_symbol(),
          NONE),
      false);

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
    Handle<Object> global_proxy(debug_context->global_proxy(), isolate);
    JSObject::AddProperty(global, debug_string, global_proxy, DONT_ENUM);
  }
  return true;
}


static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}


Genesis::ExtensionStates::ExtensionStates() : map_(HashMap::PointersMatch, 8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  i::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension), false);
  if (entry == NULL) {
    return UNVISITED;
  }
  return static_cast<ExtensionTraversalState>(
      reinterpret_cast<intptr_t>(entry->value));
}

void Genesis::ExtensionStates::set_state(RegisteredExtension* extension,
                                         ExtensionTraversalState state) {
  map_.Lookup(extension, Hash(extension), true)->value =
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
  Handle<String> source_code =
      isolate->factory()->NewExternalStringFromAscii(
          extension->source()).ToHandleChecked();
  bool result = CompileScriptCached(isolate,
                                    CStrVector(extension->name()),
                                    source_code,
                                    isolate->bootstrapper()->extensions_cache(),
                                    extension,
                                    Handle<Context>(isolate->context()),
                                    false);
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


bool Genesis::InstallJSBuiltins(Handle<JSBuiltinsObject> builtins) {
  HandleScope scope(isolate());
  for (int i = 0; i < Builtins::NumberOfJavaScriptBuiltins(); i++) {
    Builtins::JavaScript id = static_cast<Builtins::JavaScript>(i);
    Handle<Object> function_object = Object::GetProperty(
        isolate(), builtins, Builtins::GetName(id)).ToHandleChecked();
    Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
    builtins->set_javascript_builtin(id, *function);
    // TODO(mstarzinger): This is just a temporary hack to make TurboFan work,
    // the correct solution is to restore the context register after invoking
    // builtins from full-codegen.
    function->shared()->set_optimization_disabled(true);
    if (!Compiler::EnsureCompiled(function, CLEAR_EXCEPTION)) {
      return false;
    }
    builtins->set_javascript_builtin_code(id, function->shared()->code());
  }
  return true;
}


bool Genesis::ConfigureGlobalObjects(
    v8::Handle<v8::ObjectTemplate> global_proxy_template) {
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

  return true;
}


bool Genesis::ConfigureApiObject(Handle<JSObject> object,
                                 Handle<ObjectTemplateInfo> object_template) {
  DCHECK(!object_template.is_null());
  DCHECK(FunctionTemplateInfo::cast(object_template->constructor())
             ->IsTemplateFor(object->map()));;

  MaybeHandle<JSObject> maybe_obj =
      Execution::InstantiateObject(object_template);
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
  if (from->HasFastProperties()) {
    Handle<DescriptorArray> descs =
        Handle<DescriptorArray>(from->map()->instance_descriptors());
    for (int i = 0; i < from->map()->NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descs->GetDetails(i);
      switch (details.type()) {
        case FIELD: {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          DCHECK(!descs->GetDetails(i).representation().IsDouble());
          Handle<Object> value = Handle<Object>(from->RawFastPropertyAt(index),
                                                isolate());
          JSObject::AddProperty(to, key, value, details.attributes());
          break;
        }
        case CONSTANT: {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          Handle<Object> constant(descs->GetConstant(i), isolate());
          JSObject::AddProperty(to, key, constant, details.attributes());
          break;
        }
        case CALLBACKS: {
          LookupResult result(isolate());
          Handle<Name> key(Name::cast(descs->GetKey(i)), isolate());
          to->LookupOwn(key, &result);
          // If the property is already there we skip it
          if (result.IsFound()) continue;
          HandleScope inner(isolate());
          DCHECK(!to->HasFastProperties());
          // Add to dictionary.
          Handle<Object> callbacks(descs->GetCallbacksObject(i), isolate());
          PropertyDetails d = PropertyDetails(
              details.attributes(), CALLBACKS, i + 1);
          JSObject::SetNormalizedProperty(to, key, callbacks, d);
          break;
        }
        case NORMAL:
          // Do not occur since the from object has fast properties.
        case HANDLER:
        case INTERCEPTOR:
        case NONEXISTENT:
          // No element in instance descriptors have proxy or interceptor type.
          UNREACHABLE();
          break;
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
        LookupResult result(isolate());
        Handle<Name> key(Name::cast(raw_key));
        to->LookupOwn(key, &result);
        if (result.IsFound()) continue;
        // Set the property.
        Handle<Object> value = Handle<Object>(properties->ValueAt(i),
                                              isolate());
        DCHECK(!value->IsCell());
        if (value->IsPropertyCell()) {
          value = Handle<Object>(PropertyCell::cast(*value)->value(),
                                 isolate());
        }
        PropertyDetails details = properties->DetailsAt(i);
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
      : flag_(FLAG_track_double_fields) {
    if (isolate->serializer_enabled()) {
      // Disable tracking double fields because heap numbers treated as
      // immutable by the serializer.
      FLAG_track_double_fields = false;
    }
  }

  ~NoTrackDoubleFieldsForSerializerScope() {
    FLAG_track_double_fields = flag_;
  }

 private:
  bool flag_;
};


Genesis::Genesis(Isolate* isolate,
                 MaybeHandle<JSGlobalProxy> maybe_global_proxy,
                 v8::Handle<v8::ObjectTemplate> global_proxy_template,
                 v8::ExtensionConfiguration* extensions)
    : isolate_(isolate),
      active_(isolate->bootstrapper()) {
  NoTrackDoubleFieldsForSerializerScope disable_scope(isolate);
  result_ = Handle<Context>::null();
  // If V8 cannot be initialized, just return.
  if (!V8::Initialize(NULL)) return;

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) return;

  // We can only de-serialize a context if the isolate was initialized from
  // a snapshot. Otherwise we have to build the context from scratch.
  if (isolate->initialized_from_snapshot()) {
    native_context_ = Snapshot::NewContextFromSnapshot(isolate);
  } else {
    native_context_ = Handle<Context>();
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(*native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();
    Handle<GlobalObject> global_object;
    Handle<JSGlobalProxy> global_proxy = CreateNewGlobals(
        global_proxy_template, maybe_global_proxy, &global_object);

    HookUpGlobalProxy(global_object, global_proxy);
    HookUpGlobalObject(global_object);
    native_context()->builtins()->set_global_proxy(
        native_context()->global_proxy());

    if (!ConfigureGlobalObjects(global_proxy_template)) return;
  } else {
    // We get here if there was no context snapshot.
    CreateRoots();
    Handle<JSFunction> empty_function = CreateEmptyFunction(isolate);
    CreateStrictModeFunctionMaps(empty_function);
    Handle<GlobalObject> global_object;
    Handle<JSGlobalProxy> global_proxy = CreateNewGlobals(
        global_proxy_template, maybe_global_proxy, &global_object);
    HookUpGlobalProxy(global_object, global_proxy);
    InitializeGlobal(global_object, empty_function);
    InstallJSFunctionResultCaches();
    InitializeNormalizedMapCaches();
    if (!InstallNatives()) return;

    MakeFunctionInstancePrototypeWritable();

    if (!ConfigureGlobalObjects(global_proxy_template)) return;
    isolate->counters()->contexts_created_from_scratch()->Increment();
  }

  // Initialize experimental globals and install experimental natives.
  InitializeExperimentalGlobal();
  if (!InstallExperimentalNatives()) return;

  // We can't (de-)serialize typed arrays currently, but we are lucky: The state
  // of the random number generator needs no initialization during snapshot
  // creation time and we don't need trigonometric functions then.
  if (!isolate->serializer_enabled()) {
    // Initially seed the per-context random number generator using the
    // per-isolate random number generator.
    const int num_elems = 2;
    const int num_bytes = num_elems * sizeof(uint32_t);
    uint32_t* state = reinterpret_cast<uint32_t*>(malloc(num_bytes));

    do {
      isolate->random_number_generator()->NextBytes(state, num_bytes);
    } while (state[0] == 0 || state[1] == 0);

    v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(
        reinterpret_cast<v8::Isolate*>(isolate), state, num_bytes);
    Utils::OpenHandle(*buffer)->set_should_be_freed(true);
    v8::Local<v8::Uint32Array> ta = v8::Uint32Array::New(buffer, 0, num_elems);
    Handle<JSBuiltinsObject> builtins(native_context()->builtins());
    Runtime::DefineObjectProperty(builtins,
                                  factory()->InternalizeOneByteString(
                                      STATIC_ASCII_VECTOR("rngstate")),
                                  Utils::OpenHandle(*ta),
                                  NONE).Assert();

    // Initialize trigonometric lookup tables and constants.
    const int constants_size =
        ARRAY_SIZE(fdlibm::TrigonometricConstants::constants);
    const int table_num_bytes = constants_size * kDoubleSize;
    v8::Local<v8::ArrayBuffer> trig_buffer = v8::ArrayBuffer::New(
        reinterpret_cast<v8::Isolate*>(isolate),
        const_cast<double*>(fdlibm::TrigonometricConstants::constants),
        table_num_bytes);
    v8::Local<v8::Float64Array> trig_table =
        v8::Float64Array::New(trig_buffer, 0, constants_size);

    Runtime::DefineObjectProperty(
        builtins,
        factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR("kTrig")),
        Utils::OpenHandle(*trig_table), NONE).Assert();
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

} }  // namespace v8::internal

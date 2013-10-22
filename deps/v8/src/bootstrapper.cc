// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "bootstrapper.h"
#include "compiler.h"
#include "debug.h"
#include "execution.h"
#include "global-handles.h"
#include "isolate-inl.h"
#include "macro-assembler.h"
#include "natives.h"
#include "objects-visiting.h"
#include "platform.h"
#include "snapshot.h"
#include "extensions/externalize-string-extension.h"
#include "extensions/gc-extension.h"
#include "extensions/statistics-extension.h"
#include "code-stubs.h"

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
  ASSERT(0 <= index && index < Natives::GetBuiltinsCount());
  Heap* heap = isolate_->heap();
  if (heap->natives_source_cache()->get(index)->IsUndefined()) {
    // We can use external strings for the natives.
    Vector<const char> source = Natives::GetRawScriptSource(index);
    NativesExternalStringResource* resource =
        new NativesExternalStringResource(this,
                                          source.start(),
                                          source.length());
    Handle<String> source_code =
        isolate_->factory()->NewExternalStringFromAscii(resource);
    heap->natives_source_cache()->set(index, *source_code);
  }
  Handle<Object> cached_source(heap->natives_source_cache()->get(index),
                               isolate_);
  return Handle<String>::cast(cached_source);
}


void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(isolate_, create_heap_objects);
}


void Bootstrapper::InitializeOncePerProcess() {
  GCExtension::Register();
  ExternalizeStringExtension::Register();
  StatisticsExtension::Register();
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
    ASSERT(len < 20);  // Don't use this mechanism for unbounded allocations.
    for (int i = 0; i < len; i++) {
      delete delete_these_non_arrays_on_tear_down_->at(i);
      delete_these_non_arrays_on_tear_down_->at(i) = NULL;
    }
    delete delete_these_non_arrays_on_tear_down_;
    delete_these_non_arrays_on_tear_down_ = NULL;
  }

  if (delete_these_arrays_on_tear_down_ != NULL) {
    int len = delete_these_arrays_on_tear_down_->length();
    ASSERT(len < 1000);  // Don't use this mechanism for unbounded allocations.
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
          Handle<Object> global_object,
          v8::Handle<v8::ObjectTemplate> global_template,
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
  Handle<JSFunction> GetThrowTypeErrorFunction();

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
      v8::Handle<v8::ObjectTemplate> global_template,
      Handle<Object> global_object,
      Handle<GlobalObject>* global_proxy_out);
  // Hooks the given global proxy into the context.  If the context was created
  // by deserialization then this will unhook the global proxy that was
  // deserialized, leaving the GC to pick it up.
  void HookUpGlobalProxy(Handle<GlobalObject> inner_global,
                         Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the inner global that has been created by the
  // templates passed through the API.  The inner global from the snapshot is
  // detached from the other objects in the snapshot.
  void HookUpInnerGlobal(Handle<GlobalObject> inner_global);
  // New context initialization.  Used for creating a context from scratch.
  void InitializeGlobal(Handle<GlobalObject> inner_global,
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

  Handle<JSFunction> InstallTypedArray(const char* name,
      ElementsKind elementsKind);
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
  static bool InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static void InstallSpecialObjects(Handle<Context> native_context);
  bool InstallJSBuiltins(Handle<JSBuiltinsObject> builtins);
  bool ConfigureApiObject(Handle<JSObject> object,
                          Handle<ObjectTemplateInfo> object_template);
  bool ConfigureGlobalObjects(v8::Handle<v8::ObjectTemplate> global_template);

  // Migrates all properties from the 'from' object to the 'to'
  // object and overrides the prototype in 'to' with the one from
  // 'from'.
  void TransferObject(Handle<JSObject> from, Handle<JSObject> to);
  void TransferNamedProperties(Handle<JSObject> from, Handle<JSObject> to);
  void TransferIndexedProperties(Handle<JSObject> from, Handle<JSObject> to);

  enum PrototypePropertyMode {
    DONT_ADD_PROTOTYPE,
    ADD_READONLY_PROTOTYPE,
    ADD_WRITEABLE_PROTOTYPE
  };

  Handle<Map> CreateFunctionMap(PrototypePropertyMode prototype_mode);

  void SetFunctionInstanceDescriptor(Handle<Map> map,
                                     PrototypePropertyMode prototypeMode);
  void MakeFunctionInstancePrototypeWritable();

  Handle<Map> CreateStrictModeFunctionMap(
      PrototypePropertyMode prototype_mode,
      Handle<JSFunction> empty_function);

  void SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                           PrototypePropertyMode propertyMode);

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
  Handle<Map> function_map_writable_prototype_;
  Handle<Map> strict_mode_function_map_writable_prototype_;
  Handle<JSFunction> throw_type_error_function;

  BootstrapperActive active_;
  friend class Bootstrapper;
};


void Bootstrapper::Iterate(ObjectVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}


Handle<Context> Bootstrapper::CreateEnvironment(
    Handle<Object> global_object,
    v8::Handle<v8::ObjectTemplate> global_template,
    v8::ExtensionConfiguration* extensions) {
  HandleScope scope(isolate_);
  Genesis genesis(isolate_, global_object, global_template, extensions);
  Handle<Context> env = genesis.result();
  if (env.is_null() || !InstallExtensions(env, extensions)) {
    return Handle<Context>();
  }
  return scope.CloseAndEscape(env);
}


static void SetObjectPrototype(Handle<JSObject> object, Handle<Object> proto) {
  // object.__proto__ = proto;
  Factory* factory = object->GetIsolate()->factory();
  Handle<Map> old_to_map = Handle<Map>(object->map());
  Handle<Map> new_to_map = factory->CopyMap(old_to_map);
  new_to_map->set_prototype(*proto);
  object->set_map(*new_to_map);
}


void Bootstrapper::DetachGlobal(Handle<Context> env) {
  Factory* factory = env->GetIsolate()->factory();
  Handle<JSGlobalProxy> global_proxy(JSGlobalProxy::cast(env->global_proxy()));
  global_proxy->set_native_context(*factory->null_value());
  SetObjectPrototype(global_proxy, factory->null_value());
  env->set_global_proxy(env->global_object());
  env->global_object()->set_global_receiver(env->global_object());
}


void Bootstrapper::ReattachGlobal(Handle<Context> env,
                                  Handle<JSGlobalProxy> global_proxy) {
  env->global_object()->set_global_receiver(*global_proxy);
  env->set_global_proxy(*global_proxy);
  SetObjectPrototype(global_proxy, Handle<JSObject>(env->global_object()));
  global_proxy->set_native_context(*env);
}


static Handle<JSFunction> InstallFunction(Handle<JSObject> target,
                                          const char* name,
                                          InstanceType type,
                                          int instance_size,
                                          Handle<JSObject> prototype,
                                          Builtins::Name call,
                                          bool install_initial_map,
                                          bool set_instance_class_name) {
  Isolate* isolate = target->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<String> internalized_name = factory->InternalizeUtf8String(name);
  Handle<Code> call_code = Handle<Code>(isolate->builtins()->builtin(call));
  Handle<JSFunction> function = prototype.is_null() ?
    factory->NewFunctionWithoutPrototype(internalized_name, call_code) :
    factory->NewFunctionWithPrototype(internalized_name,
                                      type,
                                      instance_size,
                                      prototype,
                                      call_code,
                                      install_initial_map);
  PropertyAttributes attributes;
  if (target->IsJSBuiltinsObject()) {
    attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  } else {
    attributes = DONT_ENUM;
  }
  CHECK_NOT_EMPTY_HANDLE(isolate,
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             target, internalized_name, function, attributes));
  if (set_instance_class_name) {
    function->shared()->set_instance_class_name(*internalized_name);
  }
  function->shared()->set_native(true);
  return function;
}


void Genesis::SetFunctionInstanceDescriptor(
    Handle<Map> map, PrototypePropertyMode prototypeMode) {
  int size = (prototypeMode == DONT_ADD_PROTOTYPE) ? 4 : 5;
  Handle<DescriptorArray> descriptors(factory()->NewDescriptorArray(0, size));
  DescriptorArray::WhitenessWitness witness(*descriptors);

  Handle<Foreign> length(factory()->NewForeign(&Accessors::FunctionLength));
  Handle<Foreign> name(factory()->NewForeign(&Accessors::FunctionName));
  Handle<Foreign> args(factory()->NewForeign(&Accessors::FunctionArguments));
  Handle<Foreign> caller(factory()->NewForeign(&Accessors::FunctionCaller));
  Handle<Foreign> prototype;
  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    prototype = factory()->NewForeign(&Accessors::FunctionPrototype);
  }
  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE | READ_ONLY);
  map->set_instance_descriptors(*descriptors);

  {  // Add length.
    CallbacksDescriptor d(*factory()->length_string(), *length, attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add name.
    CallbacksDescriptor d(*factory()->name_string(), *name, attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add arguments.
    CallbacksDescriptor d(*factory()->arguments_string(), *args, attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add caller.
    CallbacksDescriptor d(*factory()->caller_string(), *caller, attribs);
    map->AppendDescriptor(&d, witness);
  }
  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    // Add prototype.
    if (prototypeMode == ADD_WRITEABLE_PROTOTYPE) {
      attribs = static_cast<PropertyAttributes>(attribs & ~READ_ONLY);
    }
    CallbacksDescriptor d(*factory()->prototype_string(), *prototype, attribs);
    map->AppendDescriptor(&d, witness);
  }
}


Handle<Map> Genesis::CreateFunctionMap(PrototypePropertyMode prototype_mode) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetFunctionInstanceDescriptor(map, prototype_mode);
  map->set_function_with_prototype(prototype_mode != DONT_ADD_PROTOTYPE);
  return map;
}


Handle<JSFunction> Genesis::CreateEmptyFunction(Isolate* isolate) {
  // Allocate the map for function instances. Maps are allocated first and their
  // prototypes patched later, once empty function is created.

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      CreateFunctionMap(DONT_ADD_PROTOTYPE);
  native_context()->set_function_without_prototype_map(
      *function_without_prototype_map);

  // Allocate the function map. This map is temporary, used only for processing
  // of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> function_map = CreateFunctionMap(ADD_READONLY_PROTOTYPE);
  native_context()->set_function_map(*function_map);

  // The final map for functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  function_map_writable_prototype_ = CreateFunctionMap(ADD_WRITEABLE_PROTOTYPE);

  Factory* factory = isolate->factory();

  Handle<String> object_name = factory->Object_string();

  {  // --- O b j e c t ---
    Handle<JSFunction> object_fun =
        factory->NewFunction(object_name, factory->null_value());
    Handle<Map> object_function_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    object_fun->set_initial_map(*object_function_map);
    object_function_map->set_constructor(*object_fun);

    native_context()->set_object_function(*object_fun);

    // Allocate a new prototype for the object function.
    Handle<JSObject> prototype = factory->NewJSObject(
        isolate->object_function(),
        TENURED);

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
  Handle<JSFunction> empty_function =
      factory->NewFunctionWithoutPrototype(empty_string, CLASSIC_MODE);

  // --- E m p t y ---
  Handle<Code> code =
      Handle<Code>(isolate->builtins()->builtin(
          Builtins::kEmptyFunction));
  empty_function->set_code(*code);
  empty_function->shared()->set_code(*code);
  Handle<String> source =
      factory->NewStringFromOneByte(STATIC_ASCII_VECTOR("() {}"));
  Handle<Script> script = factory->NewScript(source);
  script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
  empty_function->shared()->set_script(*script);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
  empty_function->shared()->DontAdaptArguments();

  // Set prototypes for the function maps.
  native_context()->function_map()->set_prototype(*empty_function);
  native_context()->function_without_prototype_map()->
      set_prototype(*empty_function);
  function_map_writable_prototype_->set_prototype(*empty_function);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_function_map = CreateFunctionMap(DONT_ADD_PROTOTYPE);
  empty_function_map->set_prototype(
      native_context()->object_function()->prototype());
  empty_function->set_map(*empty_function_map);
  return empty_function;
}


void Genesis::SetStrictFunctionInstanceDescriptor(
    Handle<Map> map, PrototypePropertyMode prototypeMode) {
  int size = (prototypeMode == DONT_ADD_PROTOTYPE) ? 4 : 5;
  Handle<DescriptorArray> descriptors(factory()->NewDescriptorArray(0, size));
  DescriptorArray::WhitenessWitness witness(*descriptors);

  Handle<Foreign> length(factory()->NewForeign(&Accessors::FunctionLength));
  Handle<Foreign> name(factory()->NewForeign(&Accessors::FunctionName));
  Handle<AccessorPair> arguments(factory()->NewAccessorPair());
  Handle<AccessorPair> caller(factory()->NewAccessorPair());
  Handle<Foreign> prototype;
  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    prototype = factory()->NewForeign(&Accessors::FunctionPrototype);
  }
  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  map->set_instance_descriptors(*descriptors);

  {  // Add length.
    CallbacksDescriptor d(*factory()->length_string(), *length, ro_attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add name.
    CallbacksDescriptor d(*factory()->name_string(), *name, rw_attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add arguments.
    CallbacksDescriptor d(*factory()->arguments_string(), *arguments,
                          rw_attribs);
    map->AppendDescriptor(&d, witness);
  }
  {  // Add caller.
    CallbacksDescriptor d(*factory()->caller_string(), *caller, rw_attribs);
    map->AppendDescriptor(&d, witness);
  }
  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    // Add prototype.
    PropertyAttributes attribs =
        prototypeMode == ADD_WRITEABLE_PROTOTYPE ? rw_attribs : ro_attribs;
    CallbacksDescriptor d(*factory()->prototype_string(), *prototype, attribs);
    map->AppendDescriptor(&d, witness);
  }
}


// ECMAScript 5th Edition, 13.2.3
Handle<JSFunction> Genesis::GetThrowTypeErrorFunction() {
  if (throw_type_error_function.is_null()) {
    Handle<String> name = factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("ThrowTypeError"));
    throw_type_error_function =
      factory()->NewFunctionWithoutPrototype(name, CLASSIC_MODE);
    Handle<Code> code(isolate()->builtins()->builtin(
        Builtins::kStrictModePoisonPill));
    throw_type_error_function->set_map(
        native_context()->function_map());
    throw_type_error_function->set_code(*code);
    throw_type_error_function->shared()->set_code(*code);
    throw_type_error_function->shared()->DontAdaptArguments();

    JSObject::PreventExtensions(throw_type_error_function);
  }
  return throw_type_error_function;
}


Handle<Map> Genesis::CreateStrictModeFunctionMap(
    PrototypePropertyMode prototype_mode,
    Handle<JSFunction> empty_function) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetStrictFunctionInstanceDescriptor(map, prototype_mode);
  map->set_function_with_prototype(prototype_mode != DONT_ADD_PROTOTYPE);
  map->set_prototype(*empty_function);
  return map;
}


void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  // Allocate map for the prototype-less strict mode instances.
  Handle<Map> strict_mode_function_without_prototype_map =
      CreateStrictModeFunctionMap(DONT_ADD_PROTOTYPE, empty);
  native_context()->set_strict_mode_function_without_prototype_map(
      *strict_mode_function_without_prototype_map);

  // Allocate map for the strict mode functions. This map is temporary, used
  // only for processing of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> strict_mode_function_map =
      CreateStrictModeFunctionMap(ADD_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_mode_function_map(
      *strict_mode_function_map);

  // The final map for the strict mode functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  strict_mode_function_map_writable_prototype_ =
      CreateStrictModeFunctionMap(ADD_WRITEABLE_PROTOTYPE, empty);

  // Complete the callbacks.
  PoisonArgumentsAndCaller(strict_mode_function_without_prototype_map);
  PoisonArgumentsAndCaller(strict_mode_function_map);
  PoisonArgumentsAndCaller(strict_mode_function_map_writable_prototype_);
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


void Genesis::PoisonArgumentsAndCaller(Handle<Map> map) {
  SetAccessors(map, factory()->arguments_string(), GetThrowTypeErrorFunction());
  SetAccessors(map, factory()->caller_string(), GetThrowTypeErrorFunction());
}


static void AddToWeakNativeContextList(Context* context) {
  ASSERT(context->IsNativeContext());
  Heap* heap = context->GetIsolate()->heap();
#ifdef DEBUG
  { // NOLINT
    ASSERT(context->get(Context::NEXT_CONTEXT_LINK)->IsUndefined());
    // Check that context is not in the list yet.
    for (Object* current = heap->native_contexts_list();
         !current->IsUndefined();
         current = Context::cast(current)->get(Context::NEXT_CONTEXT_LINK)) {
      ASSERT(current != context);
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
    v8::NeanderArray listeners;
    native_context()->set_message_listeners(*listeners.value());
  }
}


Handle<JSGlobalProxy> Genesis::CreateNewGlobals(
    v8::Handle<v8::ObjectTemplate> global_template,
    Handle<Object> global_object,
    Handle<GlobalObject>* inner_global_out) {
  // The argument global_template aka data is an ObjectTemplateInfo.
  // It has a constructor pointer that points at global_constructor which is a
  // FunctionTemplateInfo.
  // The global_constructor is used to create or reinitialize the global_proxy.
  // The global_constructor also has a prototype_template pointer that points at
  // js_global_template which is an ObjectTemplateInfo.
  // That in turn has a constructor pointer that points at
  // js_global_constructor which is a FunctionTemplateInfo.
  // js_global_constructor is used to make js_global_function
  // js_global_function is used to make the new inner_global.
  //
  // --- G l o b a l ---
  // Step 1: Create a fresh inner JSGlobalObject.
  Handle<JSFunction> js_global_function;
  Handle<ObjectTemplateInfo> js_global_template;
  if (!global_template.IsEmpty()) {
    // Get prototype template of the global_template.
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_template);
    Handle<FunctionTemplateInfo> global_constructor =
        Handle<FunctionTemplateInfo>(
            FunctionTemplateInfo::cast(data->constructor()));
    Handle<Object> proto_template(global_constructor->prototype_template(),
                                  isolate());
    if (!proto_template->IsUndefined()) {
      js_global_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_template.is_null()) {
    Handle<String> name = Handle<String>(heap()->empty_string());
    Handle<Code> code = Handle<Code>(isolate()->builtins()->builtin(
        Builtins::kIllegal));
    js_global_function =
        factory()->NewFunction(name, JS_GLOBAL_OBJECT_TYPE,
                               JSGlobalObject::kSize, code, true);
    // Change the constructor property of the prototype of the
    // hidden global function to refer to the Object function.
    Handle<JSObject> prototype =
        Handle<JSObject>(
            JSObject::cast(js_global_function->instance_prototype()));
    CHECK_NOT_EMPTY_HANDLE(isolate(),
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               prototype, factory()->constructor_string(),
                               isolate()->object_function(), NONE));
  } else {
    Handle<FunctionTemplateInfo> js_global_constructor(
        FunctionTemplateInfo::cast(js_global_template->constructor()));
    js_global_function =
        factory()->CreateApiFunction(js_global_constructor,
                                     factory()->InnerGlobalObject);
  }

  js_global_function->initial_map()->set_is_hidden_prototype();
  js_global_function->initial_map()->set_dictionary_map(true);
  Handle<GlobalObject> inner_global =
      factory()->NewGlobalObject(js_global_function);
  if (inner_global_out != NULL) {
    *inner_global_out = inner_global;
  }

  // Step 2: create or re-initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_template.IsEmpty()) {
    Handle<String> name = Handle<String>(heap()->empty_string());
    Handle<Code> code = Handle<Code>(isolate()->builtins()->builtin(
        Builtins::kIllegal));
    global_proxy_function =
        factory()->NewFunction(name, JS_GLOBAL_PROXY_TYPE,
                               JSGlobalProxy::kSize, code, true);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_template);
    Handle<FunctionTemplateInfo> global_constructor(
            FunctionTemplateInfo::cast(data->constructor()));
    global_proxy_function =
        factory()->CreateApiFunction(global_constructor,
                                     factory()->OuterGlobalObject);
  }

  Handle<String> global_name = factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("global"));
  global_proxy_function->shared()->set_instance_class_name(*global_name);
  global_proxy_function->initial_map()->set_is_access_check_needed(true);

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  if (global_object.location() != NULL) {
    ASSERT(global_object->IsJSGlobalProxy());
    return ReinitializeJSGlobalProxy(
        global_proxy_function,
        Handle<JSGlobalProxy>::cast(global_object));
  } else {
    return Handle<JSGlobalProxy>::cast(
        factory()->NewJSObject(global_proxy_function, TENURED));
  }
}


void Genesis::HookUpGlobalProxy(Handle<GlobalObject> inner_global,
                                Handle<JSGlobalProxy> global_proxy) {
  // Set the native context for the global object.
  inner_global->set_native_context(*native_context());
  inner_global->set_global_context(*native_context());
  inner_global->set_global_receiver(*global_proxy);
  global_proxy->set_native_context(*native_context());
  native_context()->set_global_proxy(*global_proxy);
}


void Genesis::HookUpInnerGlobal(Handle<GlobalObject> inner_global) {
  Handle<GlobalObject> inner_global_from_snapshot(
      GlobalObject::cast(native_context()->extension()));
  Handle<JSBuiltinsObject> builtins_global(native_context()->builtins());
  native_context()->set_extension(*inner_global);
  native_context()->set_global_object(*inner_global);
  native_context()->set_security_token(*inner_global);
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  ForceSetProperty(builtins_global,
                   factory()->InternalizeOneByteString(
                       STATIC_ASCII_VECTOR("global")),
                   inner_global,
                   attributes);
  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(*inner_global)->set_builtins(*builtins_global);
  TransferNamedProperties(inner_global_from_snapshot, inner_global);
  TransferIndexedProperties(inner_global_from_snapshot, inner_global);
}


// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpInnerGlobal.
void Genesis::InitializeGlobal(Handle<GlobalObject> inner_global,
                               Handle<JSFunction> empty_function) {
  // --- G l o b a l   C o n t e x t ---
  // Use the empty function as closure (no scope info).
  native_context()->set_closure(*empty_function);
  native_context()->set_previous(NULL);
  // Set extension and global object.
  native_context()->set_extension(*inner_global);
  native_context()->set_global_object(*inner_global);
  // Security setup: Set the security token of the global object to
  // its the inner global. This makes the security check between two
  // different contexts fail by default even in case of global
  // object reinitialization.
  native_context()->set_security_token(*inner_global);

  Isolate* isolate = inner_global->GetIsolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<String> object_name = factory->Object_string();
  CHECK_NOT_EMPTY_HANDLE(isolate,
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             inner_global, object_name,
                             isolate->object_function(), DONT_ENUM));

  Handle<JSObject> global = Handle<JSObject>(native_context()->global_object());

  // Install global Function object
  InstallFunction(global, "Function", JS_FUNCTION_TYPE, JSFunction::kSize,
                  empty_function, Builtins::kIllegal, true, true);

  {  // --- A r r a y ---
    Handle<JSFunction> array_function =
        InstallFunction(global, "Array", JS_ARRAY_TYPE, JSArray::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kArrayCode, true, true);
    array_function->shared()->DontAdaptArguments();
    array_function->shared()->set_function_data(Smi::FromInt(kArrayCode));

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared()->set_length(1);

    Handle<Map> initial_map(array_function->initial_map());

    // This assert protects an optimization in
    // HGraphBuilder::JSArrayBuilder::EmitMapCode()
    ASSERT(initial_map->elements_kind() == GetInitialFastElementsKind());

    Handle<DescriptorArray> array_descriptors(
        factory->NewDescriptorArray(0, 1));
    DescriptorArray::WhitenessWitness witness(*array_descriptors);

    Handle<Foreign> array_length(factory->NewForeign(&Accessors::ArrayLength));
    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE);
    initial_map->set_instance_descriptors(*array_descriptors);

    {  // Add length.
      CallbacksDescriptor d(*factory->length_string(), *array_length, attribs);
      array_function->initial_map()->AppendDescriptor(&d, witness);
    }

    // array_function is used internally. JS code creating array object should
    // search for the 'Array' property on the global object and use that one
    // as the constructor. 'Array' property on a global object can be
    // overwritten by JS code.
    native_context()->set_array_function(*array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(native_context(), initial_map);
    ArrayConstructorStub array_constructor_stub(isolate);
    Handle<Code> code = array_constructor_stub.GetCode(isolate);
    array_function->shared()->set_construct_stub(*code);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun =
        InstallFunction(global, "Number", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true, true);
    native_context()->set_number_function(*number_fun);
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true, true);
    native_context()->set_boolean_function(*boolean_fun);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun =
        InstallFunction(global, "String", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true, true);
    string_fun->shared()->set_construct_stub(
        isolate->builtins()->builtin(Builtins::kStringConstructCode));
    native_context()->set_string_function(*string_fun);

    Handle<Map> string_map =
        Handle<Map>(native_context()->string_function()->initial_map());
    Handle<DescriptorArray> string_descriptors(
        factory->NewDescriptorArray(0, 1));
    DescriptorArray::WhitenessWitness witness(*string_descriptors);

    Handle<Foreign> string_length(
        factory->NewForeign(&Accessors::StringLength));
    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE | READ_ONLY);
    string_map->set_instance_descriptors(*string_descriptors);

    {  // Add length.
      CallbacksDescriptor d(*factory->length_string(), *string_length, attribs);
      string_map->AppendDescriptor(&d, witness);
    }
  }

  {  // --- D a t e ---
    // Builtin functions for Date.prototype.
    Handle<JSFunction> date_fun =
        InstallFunction(global, "Date", JS_DATE_TYPE, JSDate::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true, true);

    native_context()->set_date_function(*date_fun);
  }


  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true, true);
    native_context()->set_regexp_function(*regexp_fun);

    ASSERT(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map());

    ASSERT_EQ(0, initial_map->inobject_properties());

    PropertyAttributes final =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<DescriptorArray> descriptors = factory->NewDescriptorArray(0, 5);
    DescriptorArray::WhitenessWitness witness(*descriptors);
    initial_map->set_instance_descriptors(*descriptors);

    {
      // ECMA-262, section 15.10.7.1.
      FieldDescriptor field(heap->source_string(),
                            JSRegExp::kSourceFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field, witness);
    }
    {
      // ECMA-262, section 15.10.7.2.
      FieldDescriptor field(heap->global_string(),
                            JSRegExp::kGlobalFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field, witness);
    }
    {
      // ECMA-262, section 15.10.7.3.
      FieldDescriptor field(heap->ignore_case_string(),
                            JSRegExp::kIgnoreCaseFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field, witness);
    }
    {
      // ECMA-262, section 15.10.7.4.
      FieldDescriptor field(heap->multiline_string(),
                            JSRegExp::kMultilineFieldIndex,
                            final,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field, witness);
    }
    {
      // ECMA-262, section 15.10.7.5.
      PropertyAttributes writable =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
      FieldDescriptor field(heap->last_index_string(),
                            JSRegExp::kLastIndexFieldIndex,
                            writable,
                            Representation::Tagged());
      initial_map->AppendDescriptor(&field, witness);
    }

    initial_map->set_inobject_properties(5);
    initial_map->set_pre_allocated_property_fields(5);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(
        initial_map->instance_size() + 5 * kPointerSize);
    initial_map->set_visitor_id(StaticVisitorBase::GetVisitorId(*initial_map));

    // RegExp prototype object is itself a RegExp.
    Handle<Map> proto_map = factory->CopyMap(initial_map);
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
    initial_map->set_prototype(*proto);
    factory->SetRegExpIrregexpData(Handle<JSRegExp>::cast(proto),
                                   JSRegExp::IRREGEXP, factory->empty_string(),
                                   JSRegExp::Flags(0), 0);
  }

  {  // -- J S O N
    Handle<String> name = factory->NewStringFromAscii(CStrVector("JSON"));
    Handle<JSFunction> cons = factory->NewFunction(name,
                                                   factory->the_hole_value());
    JSFunction::SetInstancePrototype(cons,
        Handle<Object>(native_context()->initial_object_prototype(), isolate));
    cons->SetInstanceClassName(*name);
    Handle<JSObject> json_object = factory->NewJSObject(cons, TENURED);
    ASSERT(json_object->IsJSObject());
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                                 global, name, json_object, DONT_ENUM));
    native_context()->set_json_object(*json_object);
  }

  { // -- A r r a y B u f f e r
    Handle<JSFunction> array_buffer_fun =
        InstallFunction(
            global, "ArrayBuffer", JS_ARRAY_BUFFER_TYPE,
            JSArrayBuffer::kSizeWithInternalFields,
            isolate->initial_object_prototype(),
            Builtins::kIllegal, true, true);
    native_context()->set_array_buffer_fun(*array_buffer_fun);
  }

  { // -- T y p e d A r r a y s
    Handle<JSFunction> int8_fun = InstallTypedArray("Int8Array",
        EXTERNAL_BYTE_ELEMENTS);
    native_context()->set_int8_array_fun(*int8_fun);
    Handle<JSFunction> uint8_fun = InstallTypedArray("Uint8Array",
        EXTERNAL_UNSIGNED_BYTE_ELEMENTS);
    native_context()->set_uint8_array_fun(*uint8_fun);
    Handle<JSFunction> int16_fun = InstallTypedArray("Int16Array",
        EXTERNAL_SHORT_ELEMENTS);
    native_context()->set_int16_array_fun(*int16_fun);
    Handle<JSFunction> uint16_fun = InstallTypedArray("Uint16Array",
        EXTERNAL_UNSIGNED_SHORT_ELEMENTS);
    native_context()->set_uint16_array_fun(*uint16_fun);
    Handle<JSFunction> int32_fun = InstallTypedArray("Int32Array",
        EXTERNAL_INT_ELEMENTS);
    native_context()->set_int32_array_fun(*int32_fun);
    Handle<JSFunction> uint32_fun = InstallTypedArray("Uint32Array",
        EXTERNAL_UNSIGNED_INT_ELEMENTS);
    native_context()->set_uint32_array_fun(*uint32_fun);
    Handle<JSFunction> float_fun = InstallTypedArray("Float32Array",
        EXTERNAL_FLOAT_ELEMENTS);
    native_context()->set_float_array_fun(*float_fun);
    Handle<JSFunction> double_fun = InstallTypedArray("Float64Array",
        EXTERNAL_DOUBLE_ELEMENTS);
    native_context()->set_double_array_fun(*double_fun);
    Handle<JSFunction> uint8c_fun = InstallTypedArray("Uint8ClampedArray",
        EXTERNAL_PIXEL_ELEMENTS);
    native_context()->set_uint8c_array_fun(*uint8c_fun);

    Handle<JSFunction> data_view_fun =
        InstallFunction(
            global, "DataView", JS_DATA_VIEW_TYPE,
            JSDataView::kSizeWithInternalFields,
            isolate->initial_object_prototype(),
            Builtins::kIllegal, true, true);
    native_context()->set_data_view_fun(*data_view_fun);
  }

  {  // --- arguments_boilerplate_
    // Make sure we can recognize argument objects at runtime.
    // This is done by introducing an anonymous function with
    // class_name equals 'Arguments'.
    Handle<String> arguments_string = factory->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("Arguments"));
    Handle<Code> code = Handle<Code>(
        isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSObject> prototype =
        Handle<JSObject>(
            JSObject::cast(native_context()->object_function()->prototype()));

    Handle<JSFunction> function =
        factory->NewFunctionWithPrototype(arguments_string,
                                          JS_OBJECT_TYPE,
                                          JSObject::kHeaderSize,
                                          prototype,
                                          code,
                                          false);
    ASSERT(!function->has_initial_map());
    function->shared()->set_instance_class_name(*arguments_string);
    function->shared()->set_expected_nof_properties(2);
    Handle<JSObject> result = factory->NewJSObject(function);

    native_context()->set_arguments_boilerplate(*result);
    // Note: length must be added as the first property and
    //       callee must be added as the second property.
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->length_string(),
                               factory->undefined_value(), DONT_ENUM,
                               Object::FORCE_TAGGED, FORCE_FIELD));
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->callee_string(),
                               factory->undefined_value(), DONT_ENUM,
                               Object::FORCE_TAGGED, FORCE_FIELD));

#ifdef DEBUG
    LookupResult lookup(isolate);
    result->LocalLookup(heap->callee_string(), &lookup);
    ASSERT(lookup.IsField());
    ASSERT(lookup.GetFieldIndex().field_index() == Heap::kArgumentsCalleeIndex);

    result->LocalLookup(heap->length_string(), &lookup);
    ASSERT(lookup.IsField());
    ASSERT(lookup.GetFieldIndex().field_index() == Heap::kArgumentsLengthIndex);

    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsCalleeIndex);
    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsLengthIndex);

    // Check the state of the object.
    ASSERT(result->HasFastProperties());
    ASSERT(result->HasFastObjectElements());
#endif
  }

  {  // --- aliased_arguments_boilerplate_
    // Set up a well-formed parameter map to make assertions happy.
    Handle<FixedArray> elements = factory->NewFixedArray(2);
    elements->set_map(heap->non_strict_arguments_elements_map());
    Handle<FixedArray> array;
    array = factory->NewFixedArray(0);
    elements->set(0, *array);
    array = factory->NewFixedArray(0);
    elements->set(1, *array);

    Handle<Map> old_map(native_context()->arguments_boilerplate()->map());
    Handle<Map> new_map = factory->CopyMap(old_map);
    new_map->set_pre_allocated_property_fields(2);
    Handle<JSObject> result = factory->NewJSObjectFromMap(new_map);
    // Set elements kind after allocating the object because
    // NewJSObjectFromMap assumes a fast elements map.
    new_map->set_elements_kind(NON_STRICT_ARGUMENTS_ELEMENTS);
    result->set_elements(*elements);
    ASSERT(result->HasNonStrictArgumentsElements());
    native_context()->set_aliased_arguments_boilerplate(*result);
  }

  {  // --- strict mode arguments boilerplate
    const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError functions.
    Handle<AccessorPair> callee = factory->NewAccessorPair();
    Handle<AccessorPair> caller = factory->NewAccessorPair();

    Handle<JSFunction> throw_function =
        GetThrowTypeErrorFunction();

    // Install the ThrowTypeError functions.
    callee->set_getter(*throw_function);
    callee->set_setter(*throw_function);
    caller->set_getter(*throw_function);
    caller->set_setter(*throw_function);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE,
                                      Heap::kArgumentsObjectSizeStrict);
    // Create the descriptor array for the arguments object.
    Handle<DescriptorArray> descriptors = factory->NewDescriptorArray(0, 3);
    DescriptorArray::WhitenessWitness witness(*descriptors);
    map->set_instance_descriptors(*descriptors);

    {  // length
      FieldDescriptor d(
          *factory->length_string(), 0, DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d, witness);
    }
    {  // callee
      CallbacksDescriptor d(*factory->callee_string(),
                            *callee,
                            attributes);
      map->AppendDescriptor(&d, witness);
    }
    {  // caller
      CallbacksDescriptor d(*factory->caller_string(),
                            *caller,
                            attributes);
      map->AppendDescriptor(&d, witness);
    }

    map->set_function_with_prototype(true);
    map->set_prototype(native_context()->object_function()->prototype());
    map->set_pre_allocated_property_fields(1);
    map->set_inobject_properties(1);

    // Copy constructor from the non-strict arguments boilerplate.
    map->set_constructor(
      native_context()->arguments_boilerplate()->map()->constructor());

    // Allocate the arguments boilerplate object.
    Handle<JSObject> result = factory->NewJSObjectFromMap(map);
    native_context()->set_strict_mode_arguments_boilerplate(*result);

    // Add length property only for strict mode boilerplate.
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->length_string(),
                               factory->undefined_value(), DONT_ENUM));

#ifdef DEBUG
    LookupResult lookup(isolate);
    result->LocalLookup(heap->length_string(), &lookup);
    ASSERT(lookup.IsField());
    ASSERT(lookup.GetFieldIndex().field_index() == Heap::kArgumentsLengthIndex);

    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsLengthIndex);

    // Check the state of the object.
    ASSERT(result->HasFastProperties());
    ASSERT(result->HasFastObjectElements());
#endif
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<Code> code = Handle<Code>(
        isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSFunction> context_extension_fun =
        factory->NewFunction(factory->empty_string(),
                             JS_CONTEXT_EXTENSION_OBJECT_TYPE,
                             JSObject::kHeaderSize,
                             code,
                             true);

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
    Handle<JSFunction> delegate =
        factory->NewFunction(factory->empty_string(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    native_context()->set_call_as_function_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<Code> code =
        Handle<Code>(isolate->builtins()->builtin(
            Builtins::kHandleApiCallAsConstructor));
    Handle<JSFunction> delegate =
        factory->NewFunction(factory->empty_string(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    native_context()->set_call_as_constructor_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  // Initialize the out of memory slot.
  native_context()->set_out_of_memory(heap->false_value());

  // Initialize the embedder data slot.
  Handle<FixedArray> embedder_data = factory->NewFixedArray(2);
  native_context()->set_embedder_data(*embedder_data);

  // Allocate the random seed slot.
  Handle<ByteArray> random_seed = factory->NewByteArray(kRandomStateSize);
  native_context()->set_random_seed(*random_seed);
}


Handle<JSFunction> Genesis::InstallTypedArray(
    const char* name, ElementsKind elementsKind) {
  Handle<JSObject> global = Handle<JSObject>(native_context()->global_object());
  Handle<JSFunction> result = InstallFunction(global, name, JS_TYPED_ARRAY_TYPE,
      JSTypedArray::kSize, isolate()->initial_object_prototype(),
      Builtins::kIllegal, false, true);

  Handle<Map> initial_map = isolate()->factory()->NewMap(
      JS_TYPED_ARRAY_TYPE, JSTypedArray::kSizeWithInternalFields, elementsKind);
  result->set_initial_map(*initial_map);
  initial_map->set_constructor(*result);
  return result;
}


void Genesis::InitializeExperimentalGlobal() {
  Handle<JSObject> global = Handle<JSObject>(native_context()->global_object());

  // TODO(mstarzinger): Move this into Genesis::InitializeGlobal once we no
  // longer need to live behind flags, so functions get added to the snapshot.

  if (FLAG_harmony_symbols) {
    // --- S y m b o l ---
    Handle<JSFunction> symbol_fun =
        InstallFunction(global, "Symbol", JS_VALUE_TYPE, JSValue::kSize,
                        isolate()->initial_object_prototype(),
                        Builtins::kIllegal, true, true);
    native_context()->set_symbol_function(*symbol_fun);
  }

  if (FLAG_harmony_collections) {
    {  // -- S e t
      InstallFunction(global, "Set", JS_SET_TYPE, JSSet::kSize,
                      isolate()->initial_object_prototype(),
                      Builtins::kIllegal, true, true);
    }
    {  // -- M a p
      InstallFunction(global, "Map", JS_MAP_TYPE, JSMap::kSize,
                      isolate()->initial_object_prototype(),
                      Builtins::kIllegal, true, true);
    }
    {  // -- W e a k M a p
      InstallFunction(global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kSize,
                      isolate()->initial_object_prototype(),
                      Builtins::kIllegal, true, true);
    }
    {  // -- W e a k S e t
      InstallFunction(global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kSize,
                      isolate()->initial_object_prototype(),
                      Builtins::kIllegal, true, true);
    }
  }

  if (FLAG_harmony_generators) {
    // Create generator meta-objects and install them on the builtins object.
    Handle<JSObject> builtins(native_context()->builtins());
    Handle<JSObject> generator_object_prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Handle<JSFunction> generator_function_prototype =
        InstallFunction(builtins, "GeneratorFunctionPrototype",
                        JS_FUNCTION_TYPE, JSFunction::kHeaderSize,
                        generator_object_prototype, Builtins::kIllegal,
                        false, false);
    InstallFunction(builtins, "GeneratorFunction",
                    JS_FUNCTION_TYPE, JSFunction::kSize,
                    generator_function_prototype, Builtins::kIllegal,
                    false, false);

    // Create maps for generator functions and their prototypes.  Store those
    // maps in the native context.
    Handle<Map> function_map(native_context()->function_map());
    Handle<Map> generator_function_map = factory()->CopyMap(function_map);
    generator_function_map->set_prototype(*generator_function_prototype);
    native_context()->set_generator_function_map(*generator_function_map);

    Handle<Map> strict_mode_function_map(
        native_context()->strict_mode_function_map());
    Handle<Map> strict_mode_generator_function_map = factory()->CopyMap(
        strict_mode_function_map);
    strict_mode_generator_function_map->set_prototype(
        *generator_function_prototype);
    native_context()->set_strict_mode_generator_function_map(
        *strict_mode_generator_function_map);

    Handle<Map> object_map(native_context()->object_function()->initial_map());
    Handle<Map> generator_object_prototype_map = factory()->CopyMap(
        object_map, 0);
    generator_object_prototype_map->set_prototype(
        *generator_object_prototype);
    native_context()->set_generator_object_prototype_map(
        *generator_object_prototype_map);

    // Create a map for generator result objects.
    ASSERT(object_map->inobject_properties() == 0);
    STATIC_ASSERT(JSGeneratorObject::kResultPropertyCount == 2);
    Handle<Map> generator_result_map = factory()->CopyMap(object_map,
        JSGeneratorObject::kResultPropertyCount);
    ASSERT(generator_result_map->inobject_properties() ==
        JSGeneratorObject::kResultPropertyCount);

    Handle<DescriptorArray> descriptors = factory()->NewDescriptorArray(0,
        JSGeneratorObject::kResultPropertyCount);
    DescriptorArray::WhitenessWitness witness(*descriptors);
    generator_result_map->set_instance_descriptors(*descriptors);

    Handle<String> value_string = factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("value"));
    FieldDescriptor value_descr(*value_string,
                                JSGeneratorObject::kResultValuePropertyIndex,
                                NONE,
                                Representation::Tagged());
    generator_result_map->AppendDescriptor(&value_descr, witness);

    Handle<String> done_string = factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("done"));
    FieldDescriptor done_descr(*done_string,
                               JSGeneratorObject::kResultDonePropertyIndex,
                               NONE,
                               Representation::Tagged());
    generator_result_map->AppendDescriptor(&done_descr, witness);

    generator_result_map->set_unused_property_fields(0);
    ASSERT_EQ(JSGeneratorObject::kResultSize,
              generator_result_map->instance_size());
    native_context()->set_generator_result_map(*generator_result_map);
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
  Handle<String> source_code =
      factory->NewStringFromAscii(
          ExperimentalNatives::GetRawScriptSource(index));
  return CompileNative(isolate, name, source_code);
}


bool Genesis::CompileNative(Isolate* isolate,
                            Vector<const char> name,
                            Handle<String> source) {
  HandleScope scope(isolate);
#ifdef ENABLE_DEBUGGER_SUPPORT
  isolate->debugger()->set_compiling_natives(true);
#endif
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
  ASSERT(isolate->has_pending_exception() != result);
  if (!result) isolate->clear_pending_exception();
#ifdef ENABLE_DEBUGGER_SUPPORT
  isolate->debugger()->set_compiling_natives(false);
#endif
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
    ASSERT(source->IsOneByteRepresentation());
    Handle<String> script_name = factory->NewStringFromUtf8(name);
    function_info = Compiler::Compile(
        source,
        script_name,
        0,
        0,
        false,
        top_context,
        extension,
        NULL,
        Handle<String>::null(),
        use_runtime_context ? NATIVES_CODE : NOT_NATIVES_CODE);
    if (function_info.is_null()) return false;
    if (cache != NULL) cache->Add(name, function_info);
  }

  // Set up the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  ASSERT(top_context->IsNativeContext());
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
  bool has_pending_exception;
  Execution::Call(isolate, fun, receiver, 0, NULL, &has_pending_exception);
  if (has_pending_exception) return false;
  return true;
}


#define INSTALL_NATIVE(Type, name, var)                                 \
  Handle<String> var##_name =                                           \
    factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR(name));     \
  Object* var##_native =                                                \
      native_context()->builtins()->GetPropertyNoExceptionThrown(       \
           *var##_name);                                                \
  native_context()->set_##var(Type::cast(var##_native));


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
}


void Genesis::InstallExperimentalNativeFunctions() {
  if (FLAG_harmony_proxies) {
    INSTALL_NATIVE(JSFunction, "DerivedHasTrap", derived_has_trap);
    INSTALL_NATIVE(JSFunction, "DerivedGetTrap", derived_get_trap);
    INSTALL_NATIVE(JSFunction, "DerivedSetTrap", derived_set_trap);
    INSTALL_NATIVE(JSFunction, "ProxyEnumerate", proxy_enumerate);
  }
  if (FLAG_harmony_observation) {
    INSTALL_NATIVE(JSFunction, "NotifyChange", observers_notify_change);
    INSTALL_NATIVE(JSFunction, "EnqueueSpliceRecord", observers_enqueue_splice);
    INSTALL_NATIVE(JSFunction, "BeginPerformSplice",
                   observers_begin_perform_splice);
    INSTALL_NATIVE(JSFunction, "EndPerformSplice",
                   observers_end_perform_splice);
    INSTALL_NATIVE(JSFunction, "DeliverChangeRecords",
                   observers_deliver_changes);
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
  Handle<JSFunction> array_function =
      InstallFunction(builtins,
                      name,
                      JS_ARRAY_TYPE,
                      JSArray::kSize,
                      isolate()->initial_object_prototype(),
                      Builtins::kInternalArrayCode,
                      true, true);
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Accessors::FunctionSetPrototype(array_function, prototype);

  InternalArrayConstructorStub internal_array_constructor_stub(isolate());
  Handle<Code> code = internal_array_constructor_stub.GetCode(isolate());
  array_function->shared()->set_construct_stub(*code);
  array_function->shared()->DontAdaptArguments();

  Handle<Map> original_map(array_function->initial_map());
  Handle<Map> initial_map = factory()->CopyMap(original_map);
  initial_map->set_elements_kind(elements_kind);
  array_function->set_initial_map(*initial_map);

  // Make "length" magic on instances.
  Handle<DescriptorArray> array_descriptors(
      factory()->NewDescriptorArray(0, 1));
  DescriptorArray::WhitenessWitness witness(*array_descriptors);

  Handle<Foreign> array_length(factory()->NewForeign(
      &Accessors::ArrayLength));
  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE);
  initial_map->set_instance_descriptors(*array_descriptors);

  {  // Add length.
    CallbacksDescriptor d(
        *factory()->length_string(), *array_length, attribs);
    array_function->initial_map()->AppendDescriptor(&d, witness);
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
  Handle<JSFunction> builtins_fun =
      factory()->NewFunction(factory()->empty_string(),
                             JS_BUILTINS_OBJECT_TYPE,
                             JSBuiltinsObject::kSize, code, true);

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
  builtins->set_global_receiver(*builtins);

  // Set up the 'global' properties of the builtins object. The
  // 'global' property that refers to the global object is the only
  // way to get from code running in the builtins context to the
  // global object.
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  Handle<String> global_string =
      factory()->InternalizeOneByteString(STATIC_ASCII_VECTOR("global"));
  Handle<Object> global_obj(native_context()->global_object(), isolate());
  CHECK_NOT_EMPTY_HANDLE(isolate(),
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             builtins, global_string, global_obj, attributes));

  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(native_context()->global_object())->
      set_builtins(*builtins);

  // Create a bridge function that has context in the native context.
  Handle<JSFunction> bridge =
      factory()->NewFunction(factory()->empty_string(),
                             factory()->undefined_value());
  ASSERT(bridge->context() == *isolate()->native_context());

  // Allocate the builtins context.
  Handle<Context> context =
    factory()->NewFunctionContext(Context::MIN_CONTEXT_SLOTS, bridge);
  context->set_global_object(*builtins);  // override builtins global object

  native_context()->set_runtime_context(*context);

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun =
        InstallFunction(builtins, "Script", JS_VALUE_TYPE, JSValue::kSize,
                        isolate()->initial_object_prototype(),
                        Builtins::kIllegal, false, false);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Accessors::FunctionSetPrototype(script_fun, prototype);
    native_context()->set_script_function(*script_fun);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());

    Handle<DescriptorArray> script_descriptors(
        factory()->NewDescriptorArray(0, 13));
    DescriptorArray::WhitenessWitness witness(*script_descriptors);

    Handle<Foreign> script_source(
        factory()->NewForeign(&Accessors::ScriptSource));
    Handle<Foreign> script_name(factory()->NewForeign(&Accessors::ScriptName));
    Handle<String> id_string(factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("id")));
    Handle<Foreign> script_id(factory()->NewForeign(&Accessors::ScriptId));
    Handle<String> line_offset_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("line_offset")));
    Handle<Foreign> script_line_offset(
        factory()->NewForeign(&Accessors::ScriptLineOffset));
    Handle<String> column_offset_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("column_offset")));
    Handle<Foreign> script_column_offset(
        factory()->NewForeign(&Accessors::ScriptColumnOffset));
    Handle<String> data_string(factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("data")));
    Handle<Foreign> script_data(factory()->NewForeign(&Accessors::ScriptData));
    Handle<String> type_string(factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("type")));
    Handle<Foreign> script_type(factory()->NewForeign(&Accessors::ScriptType));
    Handle<String> compilation_type_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("compilation_type")));
    Handle<Foreign> script_compilation_type(
        factory()->NewForeign(&Accessors::ScriptCompilationType));
    Handle<String> line_ends_string(factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("line_ends")));
    Handle<Foreign> script_line_ends(
        factory()->NewForeign(&Accessors::ScriptLineEnds));
    Handle<String> context_data_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("context_data")));
    Handle<Foreign> script_context_data(
        factory()->NewForeign(&Accessors::ScriptContextData));
    Handle<String> eval_from_script_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("eval_from_script")));
    Handle<Foreign> script_eval_from_script(
        factory()->NewForeign(&Accessors::ScriptEvalFromScript));
    Handle<String> eval_from_script_position_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("eval_from_script_position")));
    Handle<Foreign> script_eval_from_script_position(
        factory()->NewForeign(&Accessors::ScriptEvalFromScriptPosition));
    Handle<String> eval_from_function_name_string(
        factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("eval_from_function_name")));
    Handle<Foreign> script_eval_from_function_name(
        factory()->NewForeign(&Accessors::ScriptEvalFromFunctionName));
    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    script_map->set_instance_descriptors(*script_descriptors);

    {
      CallbacksDescriptor d(
          *factory()->source_string(), *script_source, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*factory()->name_string(), *script_name, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*id_string, *script_id, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*line_offset_string, *script_line_offset, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *column_offset_string, *script_column_offset, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*data_string, *script_data, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*type_string, *script_type, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *compilation_type_string, *script_compilation_type, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(*line_ends_string, *script_line_ends, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *context_data_string, *script_context_data, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *eval_from_script_string, *script_eval_from_script, attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *eval_from_script_position_string,
          *script_eval_from_script_position,
          attribs);
      script_map->AppendDescriptor(&d, witness);
    }

    {
      CallbacksDescriptor d(
          *eval_from_function_name_string,
          *script_eval_from_function_name,
          attribs);
      script_map->AppendDescriptor(&d, witness);
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
    Handle<JSFunction> opaque_reference_fun =
        InstallFunction(builtins, "OpaqueReference", JS_VALUE_TYPE,
                        JSValue::kSize,
                        isolate()->initial_object_prototype(),
                        Builtins::kIllegal, false, false);
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
  ASSERT(JSObject::cast(
      string_function->initial_map()->prototype())->HasFastProperties());
  native_context()->set_string_function_prototype_map(
      HeapObject::cast(string_function->initial_map()->prototype())->map());

  // Install Function.prototype.call and apply.
  { Handle<String> key = factory()->function_class_string();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(
            GetProperty(isolate(), isolate()->global_object(), key));
    Handle<JSObject> proto =
        Handle<JSObject>(JSObject::cast(function->instance_prototype()));

    // Install the call and the apply functions.
    Handle<JSFunction> call =
        InstallFunction(proto, "call", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::kFunctionCall,
                        false, false);
    Handle<JSFunction> apply =
        InstallFunction(proto, "apply", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::kFunctionApply,
                        false, false);

    // Make sure that Function.prototype.call appears to be compiled.
    // The code will never be called, but inline caching for call will
    // only work if it appears to be compiled.
    call->shared()->DontAdaptArguments();
    ASSERT(call->is_compiled());

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
    Handle<DescriptorArray> reresult_descriptors =
        factory()->NewDescriptorArray(0, 3);
    DescriptorArray::WhitenessWitness witness(*reresult_descriptors);
    initial_map->set_instance_descriptors(*reresult_descriptors);

    {
      JSFunction* array_function = native_context()->array_function();
      Handle<DescriptorArray> array_descriptors(
          array_function->initial_map()->instance_descriptors());
      String* length = heap()->length_string();
      int old = array_descriptors->SearchWithCache(
          length, array_function->initial_map());
      ASSERT(old != DescriptorArray::kNotFound);
      CallbacksDescriptor desc(length,
                               array_descriptors->GetValue(old),
                               array_descriptors->GetDetails(old).attributes());
      initial_map->AppendDescriptor(&desc, witness);
    }
    {
      FieldDescriptor index_field(heap()->index_string(),
                                  JSRegExpResult::kIndexIndex,
                                  NONE,
                                  Representation::Tagged());
      initial_map->AppendDescriptor(&index_field, witness);
    }

    {
      FieldDescriptor input_field(heap()->input_string(),
                                  JSRegExpResult::kInputIndex,
                                  NONE,
                                  Representation::Tagged());
      initial_map->AppendDescriptor(&input_field, witness);
    }

    initial_map->set_inobject_properties(2);
    initial_map->set_pre_allocated_property_fields(2);
    initial_map->set_unused_property_fields(0);

    native_context()->set_regexp_result_map(*initial_map);
  }

#ifdef VERIFY_HEAP
  builtins->Verify();
#endif

  return true;
}


bool Genesis::InstallExperimentalNatives() {
  for (int i = ExperimentalNatives::GetDebuggerCount();
       i < ExperimentalNatives::GetBuiltinsCount();
       i++) {
    if (FLAG_harmony_symbols &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native symbol.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_proxies &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native proxy.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_collections &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native collection.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_observation &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native object-observe.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_generators &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native generator.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_iteration &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native array-iterator.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_strings &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native harmony-string.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
    if (FLAG_harmony_arrays &&
        strcmp(ExperimentalNatives::GetScriptName(i).start(),
               "native harmony-array.js") == 0) {
      if (!CompileExperimentalBuiltin(isolate(), i)) return false;
    }
  }

  InstallExperimentalNativeFunctions();

  return true;
}


static Handle<JSObject> ResolveBuiltinIdHolder(
    Handle<Context> native_context,
    const char* holder_expr) {
  Isolate* isolate = native_context->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<GlobalObject> global(native_context->global_object());
  const char* period_pos = strchr(holder_expr, '.');
  if (period_pos == NULL) {
    return Handle<JSObject>::cast(GetProperty(
        isolate, global, factory->InternalizeUtf8String(holder_expr)));
  }
  ASSERT_EQ(".prototype", period_pos);
  Vector<const char> property(holder_expr,
                              static_cast<int>(period_pos - holder_expr));
  Handle<JSFunction> function = Handle<JSFunction>::cast(
      GetProperty(isolate, global, factory->InternalizeUtf8String(property)));
  return Handle<JSObject>(JSObject::cast(function->prototype()));
}


static void InstallBuiltinFunctionId(Handle<JSObject> holder,
                                     const char* function_name,
                                     BuiltinFunctionId id) {
  Factory* factory = holder->GetIsolate()->factory();
  Handle<String> name = factory->InternalizeUtf8String(function_name);
  Object* function_object = holder->GetProperty(*name)->ToObjectUnchecked();
  Handle<JSFunction> function(JSFunction::cast(function_object));
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
  Handle<FixedArray> array(
      factory()->NewFixedArray(NormalizedMapCache::kEntries, TENURED));
  native_context()->set_normalized_map_cache(NormalizedMapCache::cast(*array));
}


bool Bootstrapper::InstallExtensions(Handle<Context> native_context,
                                     v8::ExtensionConfiguration* extensions) {
  BootstrapperActive active(this);
  SaveContext saved_context(isolate_);
  isolate_->set_context(*native_context);
  if (!Genesis::InstallExtensions(native_context, extensions)) return false;
  Genesis::InstallSpecialObjects(native_context);
  return true;
}


void Genesis::InstallSpecialObjects(Handle<Context> native_context) {
  Isolate* isolate = native_context->GetIsolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<JSGlobalObject> global(JSGlobalObject::cast(
      native_context->global_object()));
  // Expose the natives in global if a name for it is specified.
  if (FLAG_expose_natives_as != NULL && strlen(FLAG_expose_natives_as) != 0) {
    Handle<String> natives =
        factory->InternalizeUtf8String(FLAG_expose_natives_as);
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               global, natives,
                               Handle<JSObject>(global->builtins()),
                               DONT_ENUM));
  }

  Handle<Object> Error = GetProperty(global, "Error");
  if (Error->IsJSObject()) {
    Handle<String> name = factory->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("stackTraceLimit"));
    Handle<Smi> stack_trace_limit(
        Smi::FromInt(FLAG_stack_trace_limit), isolate);
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               Handle<JSObject>::cast(Error), name,
                               stack_trace_limit, NONE));
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Expose the debug global object in global if a name for it is specified.
  if (FLAG_expose_debug_as != NULL && strlen(FLAG_expose_debug_as) != 0) {
    Debug* debug = isolate->debug();
    // If loading fails we just bail out without installing the
    // debugger but without tanking the whole context.
    if (!debug->Load()) return;
    // Set the security token for the debugger context to the same as
    // the shell native context to allow calling between these (otherwise
    // exposing debug global object doesn't make much sense).
    debug->debug_context()->set_security_token(
        native_context->security_token());

    Handle<String> debug_string =
        factory->InternalizeUtf8String(FLAG_expose_debug_as);
    Handle<Object> global_proxy(
        debug->debug_context()->global_proxy(), isolate);
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               global, debug_string, global_proxy, DONT_ENUM));
  }
#endif
}


static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}


static bool MatchRegisteredExtensions(void* key1, void* key2) {
  return key1 == key2;
}

Genesis::ExtensionStates::ExtensionStates()
  : map_(MatchRegisteredExtensions, 8) { }

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
  // Install auto extensions.
  v8::RegisteredExtension* current = v8::RegisteredExtension::first_extension();
  while (current != NULL) {
    if (current->extension()->auto_enable())
      InstallExtension(isolate, current, &extension_states);
    current = current->next();
  }

  if (FLAG_expose_gc) InstallExtension(isolate, "v8/gc", &extension_states);
  if (FLAG_expose_externalize_string) {
    InstallExtension(isolate, "v8/externalize", &extension_states);
  }
  if (FLAG_track_gc_object_stats) {
    InstallExtension(isolate, "v8/statistics", &extension_states);
  }

  if (extensions == NULL) return true;
  // Install required extensions
  int count = v8::ImplementationUtilities::GetNameCount(extensions);
  const char** names = v8::ImplementationUtilities::GetNames(extensions);
  for (int i = 0; i < count; i++) {
    if (!InstallExtension(isolate, names[i], &extension_states))
      return false;
  }

  return true;
}


// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states) {
  v8::RegisteredExtension* current = v8::RegisteredExtension::first_extension();
  // Loop until we find the relevant extension
  while (current != NULL) {
    if (strcmp(name, current->extension()->name()) == 0) break;
    current = current->next();
  }
  // Didn't find the extension; fail.
  if (current == NULL) {
    v8::Utils::ReportApiFailure(
        "v8::Context::New()", "Cannot find required extension");
    return false;
  }
  return InstallExtension(isolate, current, extension_states);
}


bool Genesis::InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states) {
  HandleScope scope(isolate);

  if (extension_states->get_state(current) == INSTALLED) return true;
  // The current node has already been visited so there must be a
  // cycle in the dependency graph; fail.
  if (extension_states->get_state(current) == VISITED) {
    v8::Utils::ReportApiFailure(
        "v8::Context::New()", "Circular extension dependency");
    return false;
  }
  ASSERT(extension_states->get_state(current) == UNVISITED);
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
  Handle<String> source_code =
      isolate->factory()->NewExternalStringFromAscii(extension->source());
  bool result = CompileScriptCached(isolate,
                                    CStrVector(extension->name()),
                                    source_code,
                                    isolate->bootstrapper()->extensions_cache(),
                                    extension,
                                    Handle<Context>(isolate->context()),
                                    false);
  ASSERT(isolate->has_pending_exception() != result);
  if (!result) {
    // We print out the name of the extension that fail to install.
    // When an error is thrown during bootstrapping we automatically print
    // the line number at which this happened to the console in the isolate
    // error throwing functionality.
    OS::PrintError("Error installing extension '%s'.\n",
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
    Handle<String> name =
        factory()->InternalizeUtf8String(Builtins::GetName(id));
    Object* function_object = builtins->GetPropertyNoExceptionThrown(*name);
    Handle<JSFunction> function
        = Handle<JSFunction>(JSFunction::cast(function_object));
    builtins->set_javascript_builtin(id, *function);
    if (!JSFunction::CompileLazy(function, CLEAR_EXCEPTION)) {
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
  Handle<JSObject> inner_global(
      JSObject::cast(native_context()->global_object()));

  if (!global_proxy_template.IsEmpty()) {
    // Configure the global proxy object.
    Handle<ObjectTemplateInfo> proxy_data =
        v8::Utils::OpenHandle(*global_proxy_template);
    if (!ConfigureApiObject(global_proxy, proxy_data)) return false;

    // Configure the inner global object.
    Handle<FunctionTemplateInfo> proxy_constructor(
        FunctionTemplateInfo::cast(proxy_data->constructor()));
    if (!proxy_constructor->prototype_template()->IsUndefined()) {
      Handle<ObjectTemplateInfo> inner_data(
          ObjectTemplateInfo::cast(proxy_constructor->prototype_template()));
      if (!ConfigureApiObject(inner_global, inner_data)) return false;
    }
  }

  SetObjectPrototype(global_proxy, inner_global);

  native_context()->set_initial_array_prototype(
      JSArray::cast(native_context()->array_function()->prototype()));

  return true;
}


bool Genesis::ConfigureApiObject(Handle<JSObject> object,
    Handle<ObjectTemplateInfo> object_template) {
  ASSERT(!object_template.is_null());
  ASSERT(object->IsInstanceOf(
      FunctionTemplateInfo::cast(object_template->constructor())));

  bool pending_exception = false;
  Handle<JSObject> obj =
      Execution::InstantiateObject(object_template, &pending_exception);
  if (pending_exception) {
    ASSERT(isolate()->has_pending_exception());
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
          int index = descs->GetFieldIndex(i);
          ASSERT(!descs->GetDetails(i).representation().IsDouble());
          Handle<Object> value = Handle<Object>(from->RawFastPropertyAt(index),
                                                isolate());
          CHECK_NOT_EMPTY_HANDLE(isolate(),
                                 JSObject::SetLocalPropertyIgnoreAttributes(
                                     to, key, value, details.attributes()));
          break;
        }
        case CONSTANT: {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
          Handle<Object> constant(descs->GetConstant(i), isolate());
          CHECK_NOT_EMPTY_HANDLE(isolate(),
                                 JSObject::SetLocalPropertyIgnoreAttributes(
                                     to, key, constant, details.attributes()));
          break;
        }
        case CALLBACKS: {
          LookupResult result(isolate());
          to->LocalLookup(descs->GetKey(i), &result);
          // If the property is already there we skip it
          if (result.IsFound()) continue;
          HandleScope inner(isolate());
          ASSERT(!to->HasFastProperties());
          // Add to dictionary.
          Handle<Name> key = Handle<Name>(descs->GetKey(i));
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
        case TRANSITION:
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
        ASSERT(raw_key->IsName());
        // If the property is already there we skip it.
        LookupResult result(isolate());
        to->LocalLookup(Name::cast(raw_key), &result);
        if (result.IsFound()) continue;
        // Set the property.
        Handle<Name> key = Handle<Name>(Name::cast(raw_key));
        Handle<Object> value = Handle<Object>(properties->ValueAt(i),
                                              isolate());
        ASSERT(!value->IsCell());
        if (value->IsPropertyCell()) {
          value = Handle<Object>(PropertyCell::cast(*value)->value(),
                                 isolate());
        }
        PropertyDetails details = properties->DetailsAt(i);
        CHECK_NOT_EMPTY_HANDLE(isolate(),
                               JSObject::SetLocalPropertyIgnoreAttributes(
                                   to, key, value, details.attributes()));
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

  ASSERT(!from->IsJSArray());
  ASSERT(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<Map> old_to_map = Handle<Map>(to->map());
  Handle<Map> new_to_map = factory()->CopyMap(old_to_map);
  new_to_map->set_prototype(from->map()->prototype());
  to->set_map(*new_to_map);
}


void Genesis::MakeFunctionInstancePrototypeWritable() {
  // The maps with writable prototype are created in CreateEmptyFunction
  // and CreateStrictModeFunctionMaps respectively. Initially the maps are
  // created with read-only prototype for JS builtins processing.
  ASSERT(!function_map_writable_prototype_.is_null());
  ASSERT(!strict_mode_function_map_writable_prototype_.is_null());

  // Replace function instance maps to make prototype writable.
  native_context()->set_function_map(*function_map_writable_prototype_);
  native_context()->set_strict_mode_function_map(
      *strict_mode_function_map_writable_prototype_);
}


Genesis::Genesis(Isolate* isolate,
                 Handle<Object> global_object,
                 v8::Handle<v8::ObjectTemplate> global_template,
                 v8::ExtensionConfiguration* extensions)
    : isolate_(isolate),
      active_(isolate->bootstrapper()) {
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
    Handle<GlobalObject> inner_global;
    Handle<JSGlobalProxy> global_proxy =
        CreateNewGlobals(global_template,
                         global_object,
                         &inner_global);

    HookUpGlobalProxy(inner_global, global_proxy);
    HookUpInnerGlobal(inner_global);

    if (!ConfigureGlobalObjects(global_template)) return;
  } else {
    // We get here if there was no context snapshot.
    CreateRoots();
    Handle<JSFunction> empty_function = CreateEmptyFunction(isolate);
    CreateStrictModeFunctionMaps(empty_function);
    Handle<GlobalObject> inner_global;
    Handle<JSGlobalProxy> global_proxy =
        CreateNewGlobals(global_template, global_object, &inner_global);
    HookUpGlobalProxy(inner_global, global_proxy);
    InitializeGlobal(inner_global, empty_function);
    InstallJSFunctionResultCaches();
    InitializeNormalizedMapCaches();
    if (!InstallNatives()) return;

    MakeFunctionInstancePrototypeWritable();

    if (!ConfigureGlobalObjects(global_template)) return;
    isolate->counters()->contexts_created_from_scratch()->Increment();
  }

  // Initialize experimental globals and install experimental natives.
  InitializeExperimentalGlobal();
  if (!InstallExperimentalNatives()) return;

  // Initially seed the per-context random number generator
  // using the per-isolate random number generator.
  uint32_t* state = reinterpret_cast<uint32_t*>(
      native_context()->random_seed()->GetDataStartAddress());
  do {
    isolate->random_number_generator()->NextBytes(state, kRandomStateSize);
  } while (state[0] == 0 || state[1] == 0);

  result_ = native_context();
}


// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() {
  return sizeof(NestingCounterType);
}


// Archive statics that are thread local.
char* Bootstrapper::ArchiveState(char* to) {
  *reinterpret_cast<NestingCounterType*>(to) = nesting_;
  nesting_ = 0;
  return to + sizeof(NestingCounterType);
}


// Restore statics that are thread local.
char* Bootstrapper::RestoreState(char* from) {
  nesting_ = *reinterpret_cast<NestingCounterType*>(from);
  return from + sizeof(NestingCounterType);
}


// Called when the top-level V8 mutex is destroyed.
void Bootstrapper::FreeThreadResources() {
  ASSERT(!IsActive());
}

} }  // namespace v8::internal

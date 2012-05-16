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


Bootstrapper::Bootstrapper()
    : nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION),
      delete_these_non_arrays_on_tear_down_(NULL),
      delete_these_arrays_on_tear_down_(NULL) {
}


Handle<String> Bootstrapper::NativesSourceLookup(int index) {
  ASSERT(0 <= index && index < Natives::GetBuiltinsCount());
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  if (heap->natives_source_cache()->get(index)->IsUndefined()) {
    // We can use external strings for the natives.
    Vector<const char> source = Natives::GetRawScriptSource(index);
    NativesExternalStringResource* resource =
        new NativesExternalStringResource(this,
                                          source.start(),
                                          source.length());
    Handle<String> source_code =
        factory->NewExternalStringFromAscii(resource);
    heap->natives_source_cache()->set(index, *source_code);
  }
  Handle<Object> cached_source(heap->natives_source_cache()->get(index));
  return Handle<String>::cast(cached_source);
}


void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(create_heap_objects);
  GCExtension::Register();
  ExternalizeStringExtension::Register();
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

  extensions_cache_.Initialize(false);  // Yes, symmetrical
}


class Genesis BASE_EMBEDDED {
 public:
  Genesis(Isolate* isolate,
          Handle<Object> global_object,
          v8::Handle<v8::ObjectTemplate> global_template,
          v8::ExtensionConfiguration* extensions);
  ~Genesis() { }

  Handle<Context> result() { return result_; }

  Genesis* previous() { return previous_; }

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Heap* heap() const { return isolate_->heap(); }

 private:
  Handle<Context> global_context_;
  Isolate* isolate_;

  // There may be more than one active genesis object: When GC is
  // triggered during environment creation there may be weak handle
  // processing callbacks which may create new environments.
  Genesis* previous_;

  Handle<Context> global_context() { return global_context_; }

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
  bool InitializeGlobal(Handle<GlobalObject> inner_global,
                        Handle<JSFunction> empty_function);
  void InitializeExperimentalGlobal();
  // Installs the contents of the native .js files on the global objects.
  // Used for creating a context from scratch.
  void InstallNativeFunctions();
  void InstallExperimentalNativeFunctions();
  bool InstallNatives();
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
  static bool InstallExtensions(Handle<Context> global_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallExtension(const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static void InstallSpecialObjects(Handle<Context> global_context);
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

  Handle<DescriptorArray> ComputeFunctionInstanceDescriptor(
      PrototypePropertyMode prototypeMode);
  void MakeFunctionInstancePrototypeWritable();

  Handle<Map> CreateStrictModeFunctionMap(
      PrototypePropertyMode prototype_mode,
      Handle<JSFunction> empty_function);

  Handle<DescriptorArray> ComputeStrictFunctionInstanceDescriptor(
      PrototypePropertyMode propertyMode);

  static bool CompileBuiltin(Isolate* isolate, int index);
  static bool CompileExperimentalBuiltin(Isolate* isolate, int index);
  static bool CompileNative(Vector<const char> name, Handle<String> source);
  static bool CompileScriptCached(Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context);

  Handle<Context> result_;

  // Function instance maps. Function literal maps are created initially with
  // a read only prototype for the processing of JS builtins. Later the function
  // instance maps are replaced in order to make prototype writable.
  // These are the final, writable prototype, maps.
  Handle<Map> function_instance_map_writable_prototype_;
  Handle<Map> strict_mode_function_instance_map_writable_prototype_;
  Handle<JSFunction> throw_type_error_function;

  BootstrapperActive active_;
  friend class Bootstrapper;
};


void Bootstrapper::Iterate(ObjectVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}


Handle<Context> Bootstrapper::CreateEnvironment(
    Isolate* isolate,
    Handle<Object> global_object,
    v8::Handle<v8::ObjectTemplate> global_template,
    v8::ExtensionConfiguration* extensions) {
  HandleScope scope;
  Handle<Context> env;
  Genesis genesis(isolate, global_object, global_template, extensions);
  env = genesis.result();
  if (!env.is_null()) {
    if (InstallExtensions(env, extensions)) {
      return env;
    }
  }
  return Handle<Context>();
}


static void SetObjectPrototype(Handle<JSObject> object, Handle<Object> proto) {
  // object.__proto__ = proto;
  Factory* factory = object->GetIsolate()->factory();
  Handle<Map> old_to_map = Handle<Map>(object->map());
  Handle<Map> new_to_map = factory->CopyMapDropTransitions(old_to_map);
  new_to_map->set_prototype(*proto);
  object->set_map(*new_to_map);
}


void Bootstrapper::DetachGlobal(Handle<Context> env) {
  Factory* factory = env->GetIsolate()->factory();
  JSGlobalProxy::cast(env->global_proxy())->set_context(*factory->null_value());
  SetObjectPrototype(Handle<JSObject>(env->global_proxy()),
                     factory->null_value());
  env->set_global_proxy(env->global());
  env->global()->set_global_receiver(env->global());
}


void Bootstrapper::ReattachGlobal(Handle<Context> env,
                                  Handle<Object> global_object) {
  ASSERT(global_object->IsJSGlobalProxy());
  Handle<JSGlobalProxy> global = Handle<JSGlobalProxy>::cast(global_object);
  env->global()->set_global_receiver(*global);
  env->set_global_proxy(*global);
  SetObjectPrototype(global, Handle<JSObject>(env->global()));
  global->set_context(*env);
}


static Handle<JSFunction> InstallFunction(Handle<JSObject> target,
                                          const char* name,
                                          InstanceType type,
                                          int instance_size,
                                          Handle<JSObject> prototype,
                                          Builtins::Name call,
                                          bool is_ecma_native) {
  Isolate* isolate = target->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<String> symbol = factory->LookupAsciiSymbol(name);
  Handle<Code> call_code = Handle<Code>(isolate->builtins()->builtin(call));
  Handle<JSFunction> function = prototype.is_null() ?
    factory->NewFunctionWithoutPrototype(symbol, call_code) :
    factory->NewFunctionWithPrototype(symbol,
                                      type,
                                      instance_size,
                                      prototype,
                                      call_code,
                                      is_ecma_native);
  PropertyAttributes attributes;
  if (target->IsJSBuiltinsObject()) {
    attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  } else {
    attributes = DONT_ENUM;
  }
  CHECK_NOT_EMPTY_HANDLE(isolate,
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             target, symbol, function, attributes));
  if (is_ecma_native) {
    function->shared()->set_instance_class_name(*symbol);
  }
  function->shared()->set_native(true);
  return function;
}


Handle<DescriptorArray> Genesis::ComputeFunctionInstanceDescriptor(
    PrototypePropertyMode prototypeMode) {
  int size = (prototypeMode == DONT_ADD_PROTOTYPE) ? 4 : 5;
  Handle<DescriptorArray> descriptors(factory()->NewDescriptorArray(size));
  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE | READ_ONLY);

  DescriptorArray::WhitenessWitness witness(*descriptors);

  {  // Add length.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionLength));
    CallbacksDescriptor d(*factory()->length_symbol(), *f, attribs);
    descriptors->Set(0, &d, witness);
  }
  {  // Add name.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionName));
    CallbacksDescriptor d(*factory()->name_symbol(), *f, attribs);
    descriptors->Set(1, &d, witness);
  }
  {  // Add arguments.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionArguments));
    CallbacksDescriptor d(*factory()->arguments_symbol(), *f, attribs);
    descriptors->Set(2, &d, witness);
  }
  {  // Add caller.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionCaller));
    CallbacksDescriptor d(*factory()->caller_symbol(), *f, attribs);
    descriptors->Set(3, &d, witness);
  }
  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    // Add prototype.
    if (prototypeMode == ADD_WRITEABLE_PROTOTYPE) {
      attribs = static_cast<PropertyAttributes>(attribs & ~READ_ONLY);
    }
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionPrototype));
    CallbacksDescriptor d(*factory()->prototype_symbol(), *f, attribs);
    descriptors->Set(4, &d, witness);
  }
  descriptors->Sort(witness);
  return descriptors;
}


Handle<Map> Genesis::CreateFunctionMap(PrototypePropertyMode prototype_mode) {
  Handle<Map> map = factory()->NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  Handle<DescriptorArray> descriptors =
      ComputeFunctionInstanceDescriptor(prototype_mode);
  map->set_instance_descriptors(*descriptors);
  map->set_function_with_prototype(prototype_mode != DONT_ADD_PROTOTYPE);
  return map;
}


Handle<JSFunction> Genesis::CreateEmptyFunction(Isolate* isolate) {
  // Allocate the map for function instances. Maps are allocated first and their
  // prototypes patched later, once empty function is created.

  // Please note that the prototype property for function instances must be
  // writable.
  Handle<Map> function_instance_map =
      CreateFunctionMap(ADD_WRITEABLE_PROTOTYPE);
  global_context()->set_function_instance_map(*function_instance_map);

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      CreateFunctionMap(DONT_ADD_PROTOTYPE);
  global_context()->set_function_without_prototype_map(
      *function_without_prototype_map);

  // Allocate the function map. This map is temporary, used only for processing
  // of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> function_map = CreateFunctionMap(ADD_READONLY_PROTOTYPE);
  global_context()->set_function_map(*function_map);

  // The final map for functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  function_instance_map_writable_prototype_ =
      CreateFunctionMap(ADD_WRITEABLE_PROTOTYPE);

  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<String> object_name = Handle<String>(heap->Object_symbol());

  {  // --- O b j e c t ---
    Handle<JSFunction> object_fun =
        factory->NewFunction(object_name, factory->null_value());
    Handle<Map> object_function_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    object_fun->set_initial_map(*object_function_map);
    object_function_map->set_constructor(*object_fun);

    global_context()->set_object_function(*object_fun);

    // Allocate a new prototype for the object function.
    Handle<JSObject> prototype = factory->NewJSObject(
        isolate->object_function(),
        TENURED);

    global_context()->set_initial_object_prototype(*prototype);
    SetPrototype(object_fun, prototype);
    object_function_map->
      set_instance_descriptors(heap->empty_descriptor_array());
  }

  // Allocate the empty function as the prototype for function ECMAScript
  // 262 15.3.4.
  Handle<String> symbol = factory->LookupAsciiSymbol("Empty");
  Handle<JSFunction> empty_function =
      factory->NewFunctionWithoutPrototype(symbol, CLASSIC_MODE);

  // --- E m p t y ---
  Handle<Code> code =
      Handle<Code>(isolate->builtins()->builtin(
          Builtins::kEmptyFunction));
  empty_function->set_code(*code);
  empty_function->shared()->set_code(*code);
  Handle<String> source = factory->NewStringFromAscii(CStrVector("() {}"));
  Handle<Script> script = factory->NewScript(source);
  script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
  empty_function->shared()->set_script(*script);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
  empty_function->shared()->DontAdaptArguments();

  // Set prototypes for the function maps.
  global_context()->function_map()->set_prototype(*empty_function);
  global_context()->function_instance_map()->set_prototype(*empty_function);
  global_context()->function_without_prototype_map()->
      set_prototype(*empty_function);
  function_instance_map_writable_prototype_->set_prototype(*empty_function);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_fm = factory->CopyMapDropDescriptors(
      function_without_prototype_map);
  empty_fm->set_instance_descriptors(
      function_without_prototype_map->instance_descriptors());
  empty_fm->set_prototype(global_context()->object_function()->prototype());
  empty_function->set_map(*empty_fm);
  return empty_function;
}


Handle<DescriptorArray> Genesis::ComputeStrictFunctionInstanceDescriptor(
    PrototypePropertyMode prototypeMode) {
  int size = (prototypeMode == DONT_ADD_PROTOTYPE) ? 4 : 5;
  Handle<DescriptorArray> descriptors(factory()->NewDescriptorArray(size));
  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE);

  DescriptorArray::WhitenessWitness witness(*descriptors);

  {  // Add length.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionLength));
    CallbacksDescriptor d(*factory()->length_symbol(), *f, attribs);
    descriptors->Set(0, &d, witness);
  }
  {  // Add name.
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionName));
    CallbacksDescriptor d(*factory()->name_symbol(), *f, attribs);
    descriptors->Set(1, &d, witness);
  }
  {  // Add arguments.
    Handle<AccessorPair> arguments(factory()->NewAccessorPair());
    CallbacksDescriptor d(*factory()->arguments_symbol(), *arguments, attribs);
    descriptors->Set(2, &d, witness);
  }
  {  // Add caller.
    Handle<AccessorPair> caller(factory()->NewAccessorPair());
    CallbacksDescriptor d(*factory()->caller_symbol(), *caller, attribs);
    descriptors->Set(3, &d, witness);
  }

  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    // Add prototype.
    if (prototypeMode != ADD_WRITEABLE_PROTOTYPE) {
      attribs = static_cast<PropertyAttributes>(attribs | READ_ONLY);
    }
    Handle<Foreign> f(factory()->NewForeign(&Accessors::FunctionPrototype));
    CallbacksDescriptor d(*factory()->prototype_symbol(), *f, attribs);
    descriptors->Set(4, &d, witness);
  }

  descriptors->Sort(witness);
  return descriptors;
}


// ECMAScript 5th Edition, 13.2.3
Handle<JSFunction> Genesis::GetThrowTypeErrorFunction() {
  if (throw_type_error_function.is_null()) {
    Handle<String> name = factory()->LookupAsciiSymbol("ThrowTypeError");
    throw_type_error_function =
      factory()->NewFunctionWithoutPrototype(name, CLASSIC_MODE);
    Handle<Code> code(isolate()->builtins()->builtin(
        Builtins::kStrictModePoisonPill));
    throw_type_error_function->set_map(
        global_context()->function_map());
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
  Handle<DescriptorArray> descriptors =
      ComputeStrictFunctionInstanceDescriptor(prototype_mode);
  map->set_instance_descriptors(*descriptors);
  map->set_function_with_prototype(prototype_mode != DONT_ADD_PROTOTYPE);
  map->set_prototype(*empty_function);
  return map;
}


void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  // Allocate map for the strict mode function instances.
  Handle<Map> strict_mode_function_instance_map =
      CreateStrictModeFunctionMap(ADD_WRITEABLE_PROTOTYPE, empty);
  global_context()->set_strict_mode_function_instance_map(
      *strict_mode_function_instance_map);

  // Allocate map for the prototype-less strict mode instances.
  Handle<Map> strict_mode_function_without_prototype_map =
      CreateStrictModeFunctionMap(DONT_ADD_PROTOTYPE, empty);
  global_context()->set_strict_mode_function_without_prototype_map(
      *strict_mode_function_without_prototype_map);

  // Allocate map for the strict mode functions. This map is temporary, used
  // only for processing of builtins.
  // Later the map is replaced with writable prototype map, allocated below.
  Handle<Map> strict_mode_function_map =
      CreateStrictModeFunctionMap(ADD_READONLY_PROTOTYPE, empty);
  global_context()->set_strict_mode_function_map(
      *strict_mode_function_map);

  // The final map for the strict mode functions. Writeable prototype.
  // This map is installed in MakeFunctionInstancePrototypeWritable.
  strict_mode_function_instance_map_writable_prototype_ =
      CreateStrictModeFunctionMap(ADD_WRITEABLE_PROTOTYPE, empty);

  // Complete the callbacks.
  PoisonArgumentsAndCaller(strict_mode_function_instance_map);
  PoisonArgumentsAndCaller(strict_mode_function_without_prototype_map);
  PoisonArgumentsAndCaller(strict_mode_function_map);
  PoisonArgumentsAndCaller(
      strict_mode_function_instance_map_writable_prototype_);
}


static void SetAccessors(Handle<Map> map,
                         Handle<String> name,
                         Handle<JSFunction> func) {
  DescriptorArray* descs = map->instance_descriptors();
  int number = descs->Search(*name);
  AccessorPair* accessors = AccessorPair::cast(descs->GetValue(number));
  accessors->set_getter(*func);
  accessors->set_setter(*func);
}


void Genesis::PoisonArgumentsAndCaller(Handle<Map> map) {
  SetAccessors(map, factory()->arguments_symbol(), GetThrowTypeErrorFunction());
  SetAccessors(map, factory()->caller_symbol(), GetThrowTypeErrorFunction());
}


static void AddToWeakGlobalContextList(Context* context) {
  ASSERT(context->IsGlobalContext());
  Heap* heap = context->GetIsolate()->heap();
#ifdef DEBUG
  { // NOLINT
    ASSERT(context->get(Context::NEXT_CONTEXT_LINK)->IsUndefined());
    // Check that context is not in the list yet.
    for (Object* current = heap->global_contexts_list();
         !current->IsUndefined();
         current = Context::cast(current)->get(Context::NEXT_CONTEXT_LINK)) {
      ASSERT(current != context);
    }
  }
#endif
  context->set(Context::NEXT_CONTEXT_LINK, heap->global_contexts_list());
  heap->set_global_contexts_list(context);
}


void Genesis::CreateRoots() {
  // Allocate the global context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // global context).
  global_context_ = Handle<Context>::cast(isolate()->global_handles()->Create(
              *factory()->NewGlobalContext()));
  AddToWeakGlobalContextList(*global_context_);
  isolate()->set_context(*global_context());

  // Allocate the message listeners object.
  {
    v8::NeanderArray listeners;
    global_context()->set_message_listeners(*listeners.value());
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
    Handle<Object> proto_template(global_constructor->prototype_template());
    if (!proto_template->IsUndefined()) {
      js_global_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_template.is_null()) {
    Handle<String> name = Handle<String>(heap()->empty_symbol());
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
                               prototype, factory()->constructor_symbol(),
                               isolate()->object_function(), NONE));
  } else {
    Handle<FunctionTemplateInfo> js_global_constructor(
        FunctionTemplateInfo::cast(js_global_template->constructor()));
    js_global_function =
        factory()->CreateApiFunction(js_global_constructor,
                                     factory()->InnerGlobalObject);
  }

  js_global_function->initial_map()->set_is_hidden_prototype();
  Handle<GlobalObject> inner_global =
      factory()->NewGlobalObject(js_global_function);
  if (inner_global_out != NULL) {
    *inner_global_out = inner_global;
  }

  // Step 2: create or re-initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_template.IsEmpty()) {
    Handle<String> name = Handle<String>(heap()->empty_symbol());
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

  Handle<String> global_name = factory()->LookupAsciiSymbol("global");
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
  // Set the global context for the global object.
  inner_global->set_global_context(*global_context());
  inner_global->set_global_receiver(*global_proxy);
  global_proxy->set_context(*global_context());
  global_context()->set_global_proxy(*global_proxy);
}


void Genesis::HookUpInnerGlobal(Handle<GlobalObject> inner_global) {
  Handle<GlobalObject> inner_global_from_snapshot(
      GlobalObject::cast(global_context_->extension()));
  Handle<JSBuiltinsObject> builtins_global(global_context_->builtins());
  global_context_->set_extension(*inner_global);
  global_context_->set_global(*inner_global);
  global_context_->set_security_token(*inner_global);
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  ForceSetProperty(builtins_global,
                   factory()->LookupAsciiSymbol("global"),
                   inner_global,
                   attributes);
  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(*inner_global)->set_builtins(*builtins_global);
  TransferNamedProperties(inner_global_from_snapshot, inner_global);
  TransferIndexedProperties(inner_global_from_snapshot, inner_global);
}


// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpInnerGlobal.
bool Genesis::InitializeGlobal(Handle<GlobalObject> inner_global,
                               Handle<JSFunction> empty_function) {
  // --- G l o b a l   C o n t e x t ---
  // Use the empty function as closure (no scope info).
  global_context()->set_closure(*empty_function);
  global_context()->set_previous(NULL);
  // Set extension and global object.
  global_context()->set_extension(*inner_global);
  global_context()->set_global(*inner_global);
  // Security setup: Set the security token of the global object to
  // its the inner global. This makes the security check between two
  // different contexts fail by default even in case of global
  // object reinitialization.
  global_context()->set_security_token(*inner_global);

  Isolate* isolate = inner_global->GetIsolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<String> object_name = Handle<String>(heap->Object_symbol());
  CHECK_NOT_EMPTY_HANDLE(isolate,
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             inner_global, object_name,
                             isolate->object_function(), DONT_ENUM));

  Handle<JSObject> global = Handle<JSObject>(global_context()->global());

  // Install global Function object
  InstallFunction(global, "Function", JS_FUNCTION_TYPE, JSFunction::kSize,
                  empty_function, Builtins::kIllegal, true);  // ECMA native.

  {  // --- A r r a y ---
    Handle<JSFunction> array_function =
        InstallFunction(global, "Array", JS_ARRAY_TYPE, JSArray::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kArrayCode, true);
    array_function->shared()->set_construct_stub(
        isolate->builtins()->builtin(Builtins::kArrayConstructCode));
    array_function->shared()->DontAdaptArguments();

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared()->set_length(1);
    Handle<DescriptorArray> array_descriptors =
        factory->CopyAppendForeignDescriptor(
            factory->empty_descriptor_array(),
            factory->length_symbol(),
            factory->NewForeign(&Accessors::ArrayLength),
            static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE));

    // array_function is used internally. JS code creating array object should
    // search for the 'Array' property on the global object and use that one
    // as the constructor. 'Array' property on a global object can be
    // overwritten by JS code.
    global_context()->set_array_function(*array_function);
    array_function->initial_map()->set_instance_descriptors(*array_descriptors);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun =
        InstallFunction(global, "Number", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true);
    global_context()->set_number_function(*number_fun);
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true);
    global_context()->set_boolean_function(*boolean_fun);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun =
        InstallFunction(global, "String", JS_VALUE_TYPE, JSValue::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true);
    string_fun->shared()->set_construct_stub(
        isolate->builtins()->builtin(Builtins::kStringConstructCode));
    global_context()->set_string_function(*string_fun);
    // Add 'length' property to strings.
    Handle<DescriptorArray> string_descriptors =
        factory->CopyAppendForeignDescriptor(
            factory->empty_descriptor_array(),
            factory->length_symbol(),
            factory->NewForeign(&Accessors::StringLength),
            static_cast<PropertyAttributes>(DONT_ENUM |
                                            DONT_DELETE |
                                            READ_ONLY));

    Handle<Map> string_map =
        Handle<Map>(global_context()->string_function()->initial_map());
    string_map->set_instance_descriptors(*string_descriptors);
  }

  {  // --- D a t e ---
    // Builtin functions for Date.prototype.
    Handle<JSFunction> date_fun =
        InstallFunction(global, "Date", JS_DATE_TYPE, JSDate::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true);

    global_context()->set_date_function(*date_fun);
  }


  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        isolate->initial_object_prototype(),
                        Builtins::kIllegal, true);
    global_context()->set_regexp_function(*regexp_fun);

    ASSERT(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map());

    ASSERT_EQ(0, initial_map->inobject_properties());

    Handle<DescriptorArray> descriptors = factory->NewDescriptorArray(5);
    DescriptorArray::WhitenessWitness witness(*descriptors);
    PropertyAttributes final =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    int enum_index = 0;
    {
      // ECMA-262, section 15.10.7.1.
      FieldDescriptor field(heap->source_symbol(),
                            JSRegExp::kSourceFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(0, &field, witness);
    }
    {
      // ECMA-262, section 15.10.7.2.
      FieldDescriptor field(heap->global_symbol(),
                            JSRegExp::kGlobalFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(1, &field, witness);
    }
    {
      // ECMA-262, section 15.10.7.3.
      FieldDescriptor field(heap->ignore_case_symbol(),
                            JSRegExp::kIgnoreCaseFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(2, &field, witness);
    }
    {
      // ECMA-262, section 15.10.7.4.
      FieldDescriptor field(heap->multiline_symbol(),
                            JSRegExp::kMultilineFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(3, &field, witness);
    }
    {
      // ECMA-262, section 15.10.7.5.
      PropertyAttributes writable =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
      FieldDescriptor field(heap->last_index_symbol(),
                            JSRegExp::kLastIndexFieldIndex,
                            writable,
                            enum_index++);
      descriptors->Set(4, &field, witness);
    }
    descriptors->SetNextEnumerationIndex(enum_index);
    descriptors->Sort(witness);

    initial_map->set_inobject_properties(5);
    initial_map->set_pre_allocated_property_fields(5);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(
        initial_map->instance_size() + 5 * kPointerSize);
    initial_map->set_instance_descriptors(*descriptors);
    initial_map->set_visitor_id(StaticVisitorBase::GetVisitorId(*initial_map));

    // RegExp prototype object is itself a RegExp.
    Handle<Map> proto_map = factory->CopyMapDropTransitions(initial_map);
    proto_map->set_prototype(global_context()->initial_object_prototype());
    Handle<JSObject> proto = factory->NewJSObjectFromMap(proto_map);
    proto->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex,
                                 heap->query_colon_symbol());
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
    { MaybeObject* result = cons->SetInstancePrototype(
        global_context()->initial_object_prototype());
      if (result->IsFailure()) return false;
    }
    cons->SetInstanceClassName(*name);
    Handle<JSObject> json_object = factory->NewJSObject(cons, TENURED);
    ASSERT(json_object->IsJSObject());
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                                 global, name, json_object, DONT_ENUM));
    global_context()->set_json_object(*json_object);
  }

  {  // --- arguments_boilerplate_
    // Make sure we can recognize argument objects at runtime.
    // This is done by introducing an anonymous function with
    // class_name equals 'Arguments'.
    Handle<String> symbol = factory->LookupAsciiSymbol("Arguments");
    Handle<Code> code = Handle<Code>(
        isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSObject> prototype =
        Handle<JSObject>(
            JSObject::cast(global_context()->object_function()->prototype()));

    Handle<JSFunction> function =
        factory->NewFunctionWithPrototype(symbol,
                                          JS_OBJECT_TYPE,
                                          JSObject::kHeaderSize,
                                          prototype,
                                          code,
                                          false);
    ASSERT(!function->has_initial_map());
    function->shared()->set_instance_class_name(*symbol);
    function->shared()->set_expected_nof_properties(2);
    Handle<JSObject> result = factory->NewJSObject(function);

    global_context()->set_arguments_boilerplate(*result);
    // Note: length must be added as the first property and
    //       callee must be added as the second property.
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->length_symbol(),
                               factory->undefined_value(), DONT_ENUM));
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->callee_symbol(),
                               factory->undefined_value(), DONT_ENUM));

#ifdef DEBUG
    LookupResult lookup(isolate);
    result->LocalLookup(heap->callee_symbol(), &lookup);
    ASSERT(lookup.IsFound() && (lookup.type() == FIELD));
    ASSERT(lookup.GetFieldIndex() == Heap::kArgumentsCalleeIndex);

    result->LocalLookup(heap->length_symbol(), &lookup);
    ASSERT(lookup.IsFound() && (lookup.type() == FIELD));
    ASSERT(lookup.GetFieldIndex() == Heap::kArgumentsLengthIndex);

    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsCalleeIndex);
    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsLengthIndex);

    // Check the state of the object.
    ASSERT(result->HasFastProperties());
    ASSERT(result->HasFastElements());
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

    Handle<Map> old_map(global_context()->arguments_boilerplate()->map());
    Handle<Map> new_map = factory->CopyMapDropTransitions(old_map);
    new_map->set_pre_allocated_property_fields(2);
    Handle<JSObject> result = factory->NewJSObjectFromMap(new_map);
    // Set elements kind after allocating the object because
    // NewJSObjectFromMap assumes a fast elements map.
    new_map->set_elements_kind(NON_STRICT_ARGUMENTS_ELEMENTS);
    result->set_elements(*elements);
    ASSERT(result->HasNonStrictArgumentsElements());
    global_context()->set_aliased_arguments_boilerplate(*result);
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

    // Create the descriptor array for the arguments object.
    Handle<DescriptorArray> descriptors = factory->NewDescriptorArray(3);
    DescriptorArray::WhitenessWitness witness(*descriptors);
    {  // length
      FieldDescriptor d(*factory->length_symbol(), 0, DONT_ENUM);
      descriptors->Set(0, &d, witness);
    }
    {  // callee
      CallbacksDescriptor d(*factory->callee_symbol(), *callee, attributes);
      descriptors->Set(1, &d, witness);
    }
    {  // caller
      CallbacksDescriptor d(*factory->caller_symbol(), *caller, attributes);
      descriptors->Set(2, &d, witness);
    }
    descriptors->Sort(witness);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE,
                                      Heap::kArgumentsObjectSizeStrict);
    map->set_instance_descriptors(*descriptors);
    map->set_function_with_prototype(true);
    map->set_prototype(global_context()->object_function()->prototype());
    map->set_pre_allocated_property_fields(1);
    map->set_inobject_properties(1);

    // Copy constructor from the non-strict arguments boilerplate.
    map->set_constructor(
      global_context()->arguments_boilerplate()->map()->constructor());

    // Allocate the arguments boilerplate object.
    Handle<JSObject> result = factory->NewJSObjectFromMap(map);
    global_context()->set_strict_mode_arguments_boilerplate(*result);

    // Add length property only for strict mode boilerplate.
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               result, factory->length_symbol(),
                               factory->undefined_value(), DONT_ENUM));

#ifdef DEBUG
    LookupResult lookup(isolate);
    result->LocalLookup(heap->length_symbol(), &lookup);
    ASSERT(lookup.IsFound() && (lookup.type() == FIELD));
    ASSERT(lookup.GetFieldIndex() == Heap::kArgumentsLengthIndex);

    ASSERT(result->map()->inobject_properties() > Heap::kArgumentsLengthIndex);

    // Check the state of the object.
    ASSERT(result->HasFastProperties());
    ASSERT(result->HasFastElements());
#endif
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<Code> code = Handle<Code>(
        isolate->builtins()->builtin(Builtins::kIllegal));
    Handle<JSFunction> context_extension_fun =
        factory->NewFunction(factory->empty_symbol(),
                             JS_CONTEXT_EXTENSION_OBJECT_TYPE,
                             JSObject::kHeaderSize,
                             code,
                             true);

    Handle<String> name = factory->LookupAsciiSymbol("context_extension");
    context_extension_fun->shared()->set_instance_class_name(*name);
    global_context()->set_context_extension_function(*context_extension_fun);
  }


  {
    // Set up the call-as-function delegate.
    Handle<Code> code =
        Handle<Code>(isolate->builtins()->builtin(
            Builtins::kHandleApiCallAsFunction));
    Handle<JSFunction> delegate =
        factory->NewFunction(factory->empty_symbol(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    global_context()->set_call_as_function_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<Code> code =
        Handle<Code>(isolate->builtins()->builtin(
            Builtins::kHandleApiCallAsConstructor));
    Handle<JSFunction> delegate =
        factory->NewFunction(factory->empty_symbol(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    global_context()->set_call_as_constructor_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  // Initialize the out of memory slot.
  global_context()->set_out_of_memory(heap->false_value());

  // Initialize the data slot.
  global_context()->set_data(heap->undefined_value());

  {
    // Initialize the random seed slot.
    Handle<ByteArray> zeroed_byte_array(
        factory->NewByteArray(kRandomStateSize));
    global_context()->set_random_seed(*zeroed_byte_array);
    memset(zeroed_byte_array->GetDataStartAddress(), 0, kRandomStateSize);
  }
  return true;
}


void Genesis::InitializeExperimentalGlobal() {
  Handle<JSObject> global = Handle<JSObject>(global_context()->global());

  // TODO(mstarzinger): Move this into Genesis::InitializeGlobal once we no
  // longer need to live behind a flag, so functions get added to the snapshot.
  if (FLAG_harmony_collections) {
    {  // -- S e t
      Handle<JSObject> prototype =
          factory()->NewJSObject(isolate()->object_function(), TENURED);
      InstallFunction(global, "Set", JS_SET_TYPE, JSSet::kSize,
                      prototype, Builtins::kIllegal, true);
    }
    {  // -- M a p
      Handle<JSObject> prototype =
          factory()->NewJSObject(isolate()->object_function(), TENURED);
      InstallFunction(global, "Map", JS_MAP_TYPE, JSMap::kSize,
                      prototype, Builtins::kIllegal, true);
    }
    {  // -- W e a k M a p
      Handle<JSObject> prototype =
          factory()->NewJSObject(isolate()->object_function(), TENURED);
      InstallFunction(global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kSize,
                      prototype, Builtins::kIllegal, true);
    }
  }
}


bool Genesis::CompileBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->NativesSourceLookup(index);
  return CompileNative(name, source_code);
}


bool Genesis::CompileExperimentalBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = ExperimentalNatives::GetScriptName(index);
  Factory* factory = isolate->factory();
  Handle<String> source_code =
      factory->NewStringFromAscii(
          ExperimentalNatives::GetRawScriptSource(index));
  return CompileNative(name, source_code);
}


bool Genesis::CompileNative(Vector<const char> name, Handle<String> source) {
  HandleScope scope;
  Isolate* isolate = source->GetIsolate();
#ifdef ENABLE_DEBUGGER_SUPPORT
  isolate->debugger()->set_compiling_natives(true);
#endif
  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(Isolate::Current());
  if (check.HasOverflowed()) return false;

  bool result = CompileScriptCached(name,
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


bool Genesis::CompileScriptCached(Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context) {
  Factory* factory = source->GetIsolate()->factory();
  HandleScope scope;
  Handle<SharedFunctionInfo> function_info;

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  if (cache == NULL || !cache->Lookup(name, &function_info)) {
    ASSERT(source->IsAsciiRepresentation());
    Handle<String> script_name = factory->NewStringFromUtf8(name);
    function_info = Compiler::Compile(
        source,
        script_name,
        0,
        0,
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
  ASSERT(top_context->IsGlobalContext());
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
                     : top_context->global());
  bool has_pending_exception;
  Execution::Call(fun, receiver, 0, NULL, &has_pending_exception);
  if (has_pending_exception) return false;
  return true;
}


#define INSTALL_NATIVE(Type, name, var)                                       \
  Handle<String> var##_name = factory()->LookupAsciiSymbol(name);             \
  Object* var##_native =                                                      \
      global_context()->builtins()->GetPropertyNoExceptionThrown(             \
           *var##_name);                                                      \
  global_context()->set_##var(Type::cast(var##_native));


void Genesis::InstallNativeFunctions() {
  HandleScope scope;
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
}

#undef INSTALL_NATIVE


bool Genesis::InstallNatives() {
  HandleScope scope;

  // Create a function for the builtins object. Allocate space for the
  // JavaScript builtins, a reference to the builtins object
  // (itself) and a reference to the global_context directly in the object.
  Handle<Code> code = Handle<Code>(
      isolate()->builtins()->builtin(Builtins::kIllegal));
  Handle<JSFunction> builtins_fun =
      factory()->NewFunction(factory()->empty_symbol(),
                             JS_BUILTINS_OBJECT_TYPE,
                             JSBuiltinsObject::kSize, code, true);

  Handle<String> name = factory()->LookupAsciiSymbol("builtins");
  builtins_fun->shared()->set_instance_class_name(*name);

  // Allocate the builtins object.
  Handle<JSBuiltinsObject> builtins =
      Handle<JSBuiltinsObject>::cast(factory()->NewGlobalObject(builtins_fun));
  builtins->set_builtins(*builtins);
  builtins->set_global_context(*global_context());
  builtins->set_global_receiver(*builtins);

  // Set up the 'global' properties of the builtins object. The
  // 'global' property that refers to the global object is the only
  // way to get from code running in the builtins context to the
  // global object.
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  Handle<String> global_symbol = factory()->LookupAsciiSymbol("global");
  Handle<Object> global_obj(global_context()->global());
  CHECK_NOT_EMPTY_HANDLE(isolate(),
                         JSObject::SetLocalPropertyIgnoreAttributes(
                             builtins, global_symbol, global_obj, attributes));

  // Set up the reference from the global object to the builtins object.
  JSGlobalObject::cast(global_context()->global())->set_builtins(*builtins);

  // Create a bridge function that has context in the global context.
  Handle<JSFunction> bridge =
      factory()->NewFunction(factory()->empty_symbol(),
                             factory()->undefined_value());
  ASSERT(bridge->context() == *isolate()->global_context());

  // Allocate the builtins context.
  Handle<Context> context =
    factory()->NewFunctionContext(Context::MIN_CONTEXT_SLOTS, bridge);
  context->set_global(*builtins);  // override builtins global object

  global_context()->set_runtime_context(*context);

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun =
        InstallFunction(builtins, "Script", JS_VALUE_TYPE, JSValue::kSize,
                        isolate()->initial_object_prototype(),
                        Builtins::kIllegal, false);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    SetPrototype(script_fun, prototype);
    global_context()->set_script_function(*script_fun);

    // Add 'source' and 'data' property to scripts.
    PropertyAttributes common_attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<Foreign> foreign_source =
        factory()->NewForeign(&Accessors::ScriptSource);
    Handle<DescriptorArray> script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            factory()->empty_descriptor_array(),
            factory()->LookupAsciiSymbol("source"),
            foreign_source,
            common_attributes);
    Handle<Foreign> foreign_name =
        factory()->NewForeign(&Accessors::ScriptName);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("name"),
            foreign_name,
            common_attributes);
    Handle<Foreign> foreign_id = factory()->NewForeign(&Accessors::ScriptId);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("id"),
            foreign_id,
            common_attributes);
    Handle<Foreign> foreign_line_offset =
        factory()->NewForeign(&Accessors::ScriptLineOffset);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("line_offset"),
            foreign_line_offset,
            common_attributes);
    Handle<Foreign> foreign_column_offset =
        factory()->NewForeign(&Accessors::ScriptColumnOffset);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("column_offset"),
            foreign_column_offset,
            common_attributes);
    Handle<Foreign> foreign_data =
        factory()->NewForeign(&Accessors::ScriptData);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("data"),
            foreign_data,
            common_attributes);
    Handle<Foreign> foreign_type =
        factory()->NewForeign(&Accessors::ScriptType);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("type"),
            foreign_type,
            common_attributes);
    Handle<Foreign> foreign_compilation_type =
        factory()->NewForeign(&Accessors::ScriptCompilationType);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("compilation_type"),
            foreign_compilation_type,
            common_attributes);
    Handle<Foreign> foreign_line_ends =
        factory()->NewForeign(&Accessors::ScriptLineEnds);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("line_ends"),
            foreign_line_ends,
            common_attributes);
    Handle<Foreign> foreign_context_data =
        factory()->NewForeign(&Accessors::ScriptContextData);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("context_data"),
            foreign_context_data,
            common_attributes);
    Handle<Foreign> foreign_eval_from_script =
        factory()->NewForeign(&Accessors::ScriptEvalFromScript);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("eval_from_script"),
            foreign_eval_from_script,
            common_attributes);
    Handle<Foreign> foreign_eval_from_script_position =
        factory()->NewForeign(&Accessors::ScriptEvalFromScriptPosition);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("eval_from_script_position"),
            foreign_eval_from_script_position,
            common_attributes);
    Handle<Foreign> foreign_eval_from_function_name =
        factory()->NewForeign(&Accessors::ScriptEvalFromFunctionName);
    script_descriptors =
        factory()->CopyAppendForeignDescriptor(
            script_descriptors,
            factory()->LookupAsciiSymbol("eval_from_function_name"),
            foreign_eval_from_function_name,
            common_attributes);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());
    script_map->set_instance_descriptors(*script_descriptors);

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
                        Builtins::kIllegal, false);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    SetPrototype(opaque_reference_fun, prototype);
    global_context()->set_opaque_reference_function(*opaque_reference_fun);
  }

  {  // --- I n t e r n a l   A r r a y ---
    // An array constructor on the builtins object that works like
    // the public Array constructor, except that its prototype
    // doesn't inherit from Object.prototype.
    // To be used only for internal work by builtins. Instances
    // must not be leaked to user code.
    Handle<JSFunction> array_function =
        InstallFunction(builtins,
                        "InternalArray",
                        JS_ARRAY_TYPE,
                        JSArray::kSize,
                        isolate()->initial_object_prototype(),
                        Builtins::kInternalArrayCode,
                        true);
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    SetPrototype(array_function, prototype);

    array_function->shared()->set_construct_stub(
        isolate()->builtins()->builtin(Builtins::kArrayConstructCode));
    array_function->shared()->DontAdaptArguments();

    // InternalArrays should not use Smi-Only array optimizations. There are too
    // many places in the C++ runtime code (e.g. RegEx) that assume that
    // elements in InternalArrays can be set to non-Smi values without going
    // through a common bottleneck that would make the SMI_ONLY -> FAST_ELEMENT
    // transition easy to trap. Moreover, they rarely are smi-only.
    MaybeObject* maybe_map =
        array_function->initial_map()->CopyDropTransitions();
    Map* new_map;
    if (!maybe_map->To<Map>(&new_map)) return false;
    new_map->set_elements_kind(FAST_ELEMENTS);
    array_function->set_initial_map(new_map);

    // Make "length" magic on instances.
    Handle<DescriptorArray> array_descriptors =
        factory()->CopyAppendForeignDescriptor(
            factory()->empty_descriptor_array(),
            factory()->length_symbol(),
            factory()->NewForeign(&Accessors::ArrayLength),
            static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE));

    array_function->initial_map()->set_instance_descriptors(
        *array_descriptors);

    global_context()->set_internal_array_function(*array_function);
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
  Handle<JSFunction> string_function(global_context()->string_function());
  ASSERT(JSObject::cast(
      string_function->initial_map()->prototype())->HasFastProperties());
  global_context()->set_string_function_prototype_map(
      HeapObject::cast(string_function->initial_map()->prototype())->map());

  // Install Function.prototype.call and apply.
  { Handle<String> key = factory()->function_class_symbol();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(GetProperty(isolate()->global(), key));
    Handle<JSObject> proto =
        Handle<JSObject>(JSObject::cast(function->instance_prototype()));

    // Install the call and the apply functions.
    Handle<JSFunction> call =
        InstallFunction(proto, "call", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::kFunctionCall,
                        false);
    Handle<JSFunction> apply =
        InstallFunction(proto, "apply", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::kFunctionApply,
                        false);

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
    Handle<JSFunction> array_constructor(global_context()->array_function());
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
        factory()->NewDescriptorArray(3);
    DescriptorArray::WhitenessWitness witness(*reresult_descriptors);

    JSFunction* array_function = global_context()->array_function();
    Handle<DescriptorArray> array_descriptors(
        array_function->initial_map()->instance_descriptors());
    int index = array_descriptors->SearchWithCache(heap()->length_symbol());
    MaybeObject* copy_result =
        reresult_descriptors->CopyFrom(0, *array_descriptors, index, witness);
    if (copy_result->IsFailure()) return false;

    int enum_index = 0;
    {
      FieldDescriptor index_field(heap()->index_symbol(),
                                  JSRegExpResult::kIndexIndex,
                                  NONE,
                                  enum_index++);
      reresult_descriptors->Set(1, &index_field, witness);
    }

    {
      FieldDescriptor input_field(heap()->input_symbol(),
                                  JSRegExpResult::kInputIndex,
                                  NONE,
                                  enum_index++);
      reresult_descriptors->Set(2, &input_field, witness);
    }
    reresult_descriptors->Sort(witness);

    initial_map->set_inobject_properties(2);
    initial_map->set_pre_allocated_property_fields(2);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_descriptors(*reresult_descriptors);

    global_context()->set_regexp_result_map(*initial_map);
  }

#ifdef DEBUG
  builtins->Verify();
#endif

  return true;
}


bool Genesis::InstallExperimentalNatives() {
  for (int i = ExperimentalNatives::GetDebuggerCount();
       i < ExperimentalNatives::GetBuiltinsCount();
       i++) {
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
  }

  InstallExperimentalNativeFunctions();

  return true;
}


static Handle<JSObject> ResolveBuiltinIdHolder(
    Handle<Context> global_context,
    const char* holder_expr) {
  Factory* factory = global_context->GetIsolate()->factory();
  Handle<GlobalObject> global(global_context->global());
  const char* period_pos = strchr(holder_expr, '.');
  if (period_pos == NULL) {
    return Handle<JSObject>::cast(
        GetProperty(global, factory->LookupAsciiSymbol(holder_expr)));
  }
  ASSERT_EQ(".prototype", period_pos);
  Vector<const char> property(holder_expr,
                              static_cast<int>(period_pos - holder_expr));
  Handle<JSFunction> function = Handle<JSFunction>::cast(
      GetProperty(global, factory->LookupSymbol(property)));
  return Handle<JSObject>(JSObject::cast(function->prototype()));
}


static void InstallBuiltinFunctionId(Handle<JSObject> holder,
                                     const char* function_name,
                                     BuiltinFunctionId id) {
  Factory* factory = holder->GetIsolate()->factory();
  Handle<String> name = factory->LookupAsciiSymbol(function_name);
  Object* function_object = holder->GetProperty(*name)->ToObjectUnchecked();
  Handle<JSFunction> function(JSFunction::cast(function_object));
  function->shared()->set_function_data(Smi::FromInt(id));
}


void Genesis::InstallBuiltinFunctionIds() {
  HandleScope scope;
#define INSTALL_BUILTIN_ID(holder_expr, fun_name, name) \
  {                                                     \
    Handle<JSObject> holder = ResolveBuiltinIdHolder(   \
        global_context(), #holder_expr);                \
    BuiltinFunctionId id = k##name;                     \
    InstallBuiltinFunctionId(holder, #fun_name, id);    \
  }
  FUNCTIONS_WITH_ID_LIST(INSTALL_BUILTIN_ID)
#undef INSTALL_BUILTIN_ID
}


// Do not forget to update macros.py with named constant
// of cache id.
#define JSFUNCTION_RESULT_CACHE_LIST(F) \
  F(16, global_context()->regexp_function())


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

  Handle<FixedArray> caches = FACTORY->NewFixedArray(kNumberOfCaches, TENURED);

  int index = 0;

#define F(size, func) do {                                              \
    FixedArray* cache = CreateCache((size), Handle<JSFunction>(func));  \
    caches->set(index++, cache);                                        \
  } while (false)

  JSFUNCTION_RESULT_CACHE_LIST(F);

#undef F

  global_context()->set_jsfunction_result_caches(*caches);
}


void Genesis::InitializeNormalizedMapCaches() {
  Handle<FixedArray> array(
      FACTORY->NewFixedArray(NormalizedMapCache::kEntries, TENURED));
  global_context()->set_normalized_map_cache(NormalizedMapCache::cast(*array));
}


bool Bootstrapper::InstallExtensions(Handle<Context> global_context,
                                     v8::ExtensionConfiguration* extensions) {
  Isolate* isolate = global_context->GetIsolate();
  BootstrapperActive active;
  SaveContext saved_context(isolate);
  isolate->set_context(*global_context);
  if (!Genesis::InstallExtensions(global_context, extensions)) return false;
  Genesis::InstallSpecialObjects(global_context);
  return true;
}


void Genesis::InstallSpecialObjects(Handle<Context> global_context) {
  Isolate* isolate = global_context->GetIsolate();
  Factory* factory = isolate->factory();
  HandleScope scope;
  Handle<JSGlobalObject> global(JSGlobalObject::cast(global_context->global()));
  // Expose the natives in global if a name for it is specified.
  if (FLAG_expose_natives_as != NULL && strlen(FLAG_expose_natives_as) != 0) {
    Handle<String> natives = factory->LookupAsciiSymbol(FLAG_expose_natives_as);
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               global, natives,
                               Handle<JSObject>(global->builtins()),
                               DONT_ENUM));
  }

  Handle<Object> Error = GetProperty(global, "Error");
  if (Error->IsJSObject()) {
    Handle<String> name = factory->LookupAsciiSymbol("stackTraceLimit");
    Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit));
    CHECK_NOT_EMPTY_HANDLE(isolate,
                           JSObject::SetLocalPropertyIgnoreAttributes(
                               Handle<JSObject>::cast(Error), name,
                               stack_trace_limit, NONE));
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Expose the debug global object in global if a name for it is specified.
  if (FLAG_expose_debug_as != NULL && strlen(FLAG_expose_debug_as) != 0) {
    Debug* debug = Isolate::Current()->debug();
    // If loading fails we just bail out without installing the
    // debugger but without tanking the whole context.
    if (!debug->Load()) return;
    // Set the security token for the debugger context to the same as
    // the shell global context to allow calling between these (otherwise
    // exposing debug global object doesn't make much sense).
    debug->debug_context()->set_security_token(
        global_context->security_token());

    Handle<String> debug_string =
        factory->LookupAsciiSymbol(FLAG_expose_debug_as);
    Handle<Object> global_proxy(debug->debug_context()->global_proxy());
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

bool Genesis::InstallExtensions(Handle<Context> global_context,
                                v8::ExtensionConfiguration* extensions) {
  // TODO(isolates): Extensions on multiple isolates may take a little more
  //                 effort. (The external API reads 'ignore'-- does that mean
  //                 we can break the interface?)


  ExtensionStates extension_states;  // All extensions have state UNVISITED.
  // Install auto extensions.
  v8::RegisteredExtension* current = v8::RegisteredExtension::first_extension();
  while (current != NULL) {
    if (current->extension()->auto_enable())
      InstallExtension(current, &extension_states);
    current = current->next();
  }

  if (FLAG_expose_gc) InstallExtension("v8/gc", &extension_states);
  if (FLAG_expose_externalize_string) {
    InstallExtension("v8/externalize", &extension_states);
  }

  if (extensions == NULL) return true;
  // Install required extensions
  int count = v8::ImplementationUtilities::GetNameCount(extensions);
  const char** names = v8::ImplementationUtilities::GetNames(extensions);
  for (int i = 0; i < count; i++) {
    if (!InstallExtension(names[i], &extension_states))
      return false;
  }

  return true;
}


// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(const char* name,
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
  return InstallExtension(current, extension_states);
}


bool Genesis::InstallExtension(v8::RegisteredExtension* current,
                               ExtensionStates* extension_states) {
  HandleScope scope;

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
    if (!InstallExtension(extension->dependencies()[i], extension_states))
      return false;
  }
  Isolate* isolate = Isolate::Current();
  Handle<String> source_code =
      isolate->factory()->NewExternalStringFromAscii(extension->source());
  bool result = CompileScriptCached(
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
  HandleScope scope;
  Factory* factory = builtins->GetIsolate()->factory();
  for (int i = 0; i < Builtins::NumberOfJavaScriptBuiltins(); i++) {
    Builtins::JavaScript id = static_cast<Builtins::JavaScript>(i);
    Handle<String> name = factory->LookupAsciiSymbol(Builtins::GetName(id));
    Object* function_object = builtins->GetPropertyNoExceptionThrown(*name);
    Handle<JSFunction> function
        = Handle<JSFunction>(JSFunction::cast(function_object));
    builtins->set_javascript_builtin(id, *function);
    Handle<SharedFunctionInfo> shared
        = Handle<SharedFunctionInfo>(function->shared());
    if (!SharedFunctionInfo::EnsureCompiled(shared, CLEAR_EXCEPTION)) {
      return false;
    }
    // Set the code object on the function object.
    function->ReplaceCode(function->shared()->code());
    builtins->set_javascript_builtin_code(id, shared->code());
  }
  return true;
}


bool Genesis::ConfigureGlobalObjects(
    v8::Handle<v8::ObjectTemplate> global_proxy_template) {
  Handle<JSObject> global_proxy(
      JSObject::cast(global_context()->global_proxy()));
  Handle<JSObject> inner_global(JSObject::cast(global_context()->global()));

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
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      PropertyDetails details = descs->GetDetails(i);
      switch (details.type()) {
        case FIELD: {
          HandleScope inner;
          Handle<String> key = Handle<String>(descs->GetKey(i));
          int index = descs->GetFieldIndex(i);
          Handle<Object> value = Handle<Object>(from->FastPropertyAt(index));
          CHECK_NOT_EMPTY_HANDLE(to->GetIsolate(),
                                 JSObject::SetLocalPropertyIgnoreAttributes(
                                     to, key, value, details.attributes()));
          break;
        }
        case CONSTANT_FUNCTION: {
          HandleScope inner;
          Handle<String> key = Handle<String>(descs->GetKey(i));
          Handle<JSFunction> fun =
              Handle<JSFunction>(descs->GetConstantFunction(i));
          CHECK_NOT_EMPTY_HANDLE(to->GetIsolate(),
                                 JSObject::SetLocalPropertyIgnoreAttributes(
                                     to, key, fun, details.attributes()));
          break;
        }
        case CALLBACKS: {
          LookupResult result(isolate());
          to->LocalLookup(descs->GetKey(i), &result);
          // If the property is already there we skip it
          if (result.IsProperty()) continue;
          HandleScope inner;
          ASSERT(!to->HasFastProperties());
          // Add to dictionary.
          Handle<String> key = Handle<String>(descs->GetKey(i));
          Handle<Object> callbacks(descs->GetCallbacksObject(i));
          PropertyDetails d =
              PropertyDetails(details.attributes(), CALLBACKS, details.index());
          JSObject::SetNormalizedProperty(to, key, callbacks, d);
          break;
        }
        case MAP_TRANSITION:
        case ELEMENTS_TRANSITION:
        case CONSTANT_TRANSITION:
        case NULL_DESCRIPTOR:
          // Ignore non-properties.
          break;
        case NORMAL:
          // Do not occur since the from object has fast properties.
        case HANDLER:
        case INTERCEPTOR:
          // No element in instance descriptors have proxy or interceptor type.
          UNREACHABLE();
          break;
      }
    }
  } else {
    Handle<StringDictionary> properties =
        Handle<StringDictionary>(from->property_dictionary());
    int capacity = properties->Capacity();
    for (int i = 0; i < capacity; i++) {
      Object* raw_key(properties->KeyAt(i));
      if (properties->IsKey(raw_key)) {
        ASSERT(raw_key->IsString());
        // If the property is already there we skip it.
        LookupResult result(isolate());
        to->LocalLookup(String::cast(raw_key), &result);
        if (result.IsProperty()) continue;
        // Set the property.
        Handle<String> key = Handle<String>(String::cast(raw_key));
        Handle<Object> value = Handle<Object>(properties->ValueAt(i));
        if (value->IsJSGlobalPropertyCell()) {
          value = Handle<Object>(JSGlobalPropertyCell::cast(*value)->value());
        }
        PropertyDetails details = properties->DetailsAt(i);
        CHECK_NOT_EMPTY_HANDLE(to->GetIsolate(),
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
  Handle<FixedArray> to_elements = FACTORY->CopyFixedArray(from_elements);
  to->set_elements(*to_elements);
}


void Genesis::TransferObject(Handle<JSObject> from, Handle<JSObject> to) {
  HandleScope outer;
  Factory* factory = from->GetIsolate()->factory();

  ASSERT(!from->IsJSArray());
  ASSERT(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<Map> old_to_map = Handle<Map>(to->map());
  Handle<Map> new_to_map = factory->CopyMapDropTransitions(old_to_map);
  new_to_map->set_prototype(from->map()->prototype());
  to->set_map(*new_to_map);
}


void Genesis::MakeFunctionInstancePrototypeWritable() {
  // The maps with writable prototype are created in CreateEmptyFunction
  // and CreateStrictModeFunctionMaps respectively. Initially the maps are
  // created with read-only prototype for JS builtins processing.
  ASSERT(!function_instance_map_writable_prototype_.is_null());
  ASSERT(!strict_mode_function_instance_map_writable_prototype_.is_null());

  // Replace function instance maps to make prototype writable.
  global_context()->set_function_map(
    *function_instance_map_writable_prototype_);
  global_context()->set_strict_mode_function_map(
    *strict_mode_function_instance_map_writable_prototype_);
}


Genesis::Genesis(Isolate* isolate,
                 Handle<Object> global_object,
                 v8::Handle<v8::ObjectTemplate> global_template,
                 v8::ExtensionConfiguration* extensions) : isolate_(isolate) {
  result_ = Handle<Context>::null();
  // If V8 isn't running and cannot be initialized, just return.
  if (!V8::IsRunning() && !V8::Initialize(NULL)) return;

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  HandleScope scope;
  SaveContext saved_context(isolate);

  // During genesis, the boilerplate for stack overflow won't work until the
  // environment has been at least partially initialized. Add a stack check
  // before entering JS code to catch overflow early.
  StackLimitCheck check(Isolate::Current());
  if (check.HasOverflowed()) return;

  Handle<Context> new_context = Snapshot::NewContextFromSnapshot();
  if (!new_context.is_null()) {
    global_context_ =
        Handle<Context>::cast(isolate->global_handles()->Create(*new_context));
    AddToWeakGlobalContextList(*global_context_);
    isolate->set_context(*global_context_);
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
    if (!InitializeGlobal(inner_global, empty_function)) return;
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

  result_ = global_context_;
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

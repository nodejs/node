// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "macro-assembler.h"
#include "natives.h"
#include "objects-visiting.h"
#include "snapshot.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// A SourceCodeCache uses a FixedArray to store pairs of
// (AsciiString*, JSFunction*), mapping names of native code files
// (runtime.js, etc.) to precompiled functions. Instead of mapping
// names to functions it might make sense to let the JS2C tool
// generate an index for each native JS file.
class SourceCodeCache BASE_EMBEDDED {
 public:
  explicit SourceCodeCache(Script::Type type): type_(type), cache_(NULL) { }

  void Initialize(bool create_heap_objects) {
    cache_ = create_heap_objects ? Heap::empty_fixed_array() : NULL;
  }

  void Iterate(ObjectVisitor* v) {
    v->VisitPointer(BitCast<Object**>(&cache_));
  }


  bool Lookup(Vector<const char> name, Handle<SharedFunctionInfo>* handle) {
    for (int i = 0; i < cache_->length(); i+=2) {
      SeqAsciiString* str = SeqAsciiString::cast(cache_->get(i));
      if (str->IsEqualTo(name)) {
        *handle = Handle<SharedFunctionInfo>(
            SharedFunctionInfo::cast(cache_->get(i + 1)));
        return true;
      }
    }
    return false;
  }


  void Add(Vector<const char> name, Handle<SharedFunctionInfo> shared) {
    HandleScope scope;
    int length = cache_->length();
    Handle<FixedArray> new_array =
        Factory::NewFixedArray(length + 2, TENURED);
    cache_->CopyTo(0, *new_array, 0, cache_->length());
    cache_ = *new_array;
    Handle<String> str = Factory::NewStringFromAscii(name, TENURED);
    cache_->set(length, *str);
    cache_->set(length + 1, *shared);
    Script::cast(shared->script())->set_type(Smi::FromInt(type_));
  }

 private:
  Script::Type type_;
  FixedArray* cache_;
  DISALLOW_COPY_AND_ASSIGN(SourceCodeCache);
};

static SourceCodeCache extensions_cache(Script::TYPE_EXTENSION);
// This is for delete, not delete[].
static List<char*>* delete_these_non_arrays_on_tear_down = NULL;
// This is for delete[]
static List<char*>* delete_these_arrays_on_tear_down = NULL;


NativesExternalStringResource::NativesExternalStringResource(const char* source)
    : data_(source), length_(StrLength(source)) {
  if (delete_these_non_arrays_on_tear_down == NULL) {
    delete_these_non_arrays_on_tear_down = new List<char*>(2);
  }
  // The resources are small objects and we only make a fixed number of
  // them, but let's clean them up on exit for neatness.
  delete_these_non_arrays_on_tear_down->
      Add(reinterpret_cast<char*>(this));
}


Handle<String> Bootstrapper::NativesSourceLookup(int index) {
  ASSERT(0 <= index && index < Natives::GetBuiltinsCount());
  if (Heap::natives_source_cache()->get(index)->IsUndefined()) {
    if (!Snapshot::IsEnabled() || FLAG_new_snapshot) {
      // We can use external strings for the natives.
      NativesExternalStringResource* resource =
          new NativesExternalStringResource(
              Natives::GetScriptSource(index).start());
      Handle<String> source_code =
          Factory::NewExternalStringFromAscii(resource);
      Heap::natives_source_cache()->set(index, *source_code);
    } else {
      // Old snapshot code can't cope with external strings at all.
      Handle<String> source_code =
        Factory::NewStringFromAscii(Natives::GetScriptSource(index));
      Heap::natives_source_cache()->set(index, *source_code);
    }
  }
  Handle<Object> cached_source(Heap::natives_source_cache()->get(index));
  return Handle<String>::cast(cached_source);
}


void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache.Initialize(create_heap_objects);
}


char* Bootstrapper::AllocateAutoDeletedArray(int bytes) {
  char* memory = new char[bytes];
  if (memory != NULL) {
    if (delete_these_arrays_on_tear_down == NULL) {
      delete_these_arrays_on_tear_down = new List<char*>(2);
    }
    delete_these_arrays_on_tear_down->Add(memory);
  }
  return memory;
}


void Bootstrapper::TearDown() {
  if (delete_these_non_arrays_on_tear_down != NULL) {
    int len = delete_these_non_arrays_on_tear_down->length();
    ASSERT(len < 20);  // Don't use this mechanism for unbounded allocations.
    for (int i = 0; i < len; i++) {
      delete delete_these_non_arrays_on_tear_down->at(i);
      delete_these_non_arrays_on_tear_down->at(i) = NULL;
    }
    delete delete_these_non_arrays_on_tear_down;
    delete_these_non_arrays_on_tear_down = NULL;
  }

  if (delete_these_arrays_on_tear_down != NULL) {
    int len = delete_these_arrays_on_tear_down->length();
    ASSERT(len < 1000);  // Don't use this mechanism for unbounded allocations.
    for (int i = 0; i < len; i++) {
      delete[] delete_these_arrays_on_tear_down->at(i);
      delete_these_arrays_on_tear_down->at(i) = NULL;
    }
    delete delete_these_arrays_on_tear_down;
    delete_these_arrays_on_tear_down = NULL;
  }

  extensions_cache.Initialize(false);  // Yes, symmetrical
}


class Genesis BASE_EMBEDDED {
 public:
  Genesis(Handle<Object> global_object,
          v8::Handle<v8::ObjectTemplate> global_template,
          v8::ExtensionConfiguration* extensions);
  ~Genesis() { }

  Handle<Context> result() { return result_; }

  Genesis* previous() { return previous_; }

 private:
  Handle<Context> global_context_;

  // There may be more than one active genesis object: When GC is
  // triggered during environment creation there may be weak handle
  // processing callbacks which may create new environments.
  Genesis* previous_;

  Handle<Context> global_context() { return global_context_; }

  // Creates some basic objects. Used for creating a context from scratch.
  void CreateRoots();
  // Creates the empty function.  Used for creating a context from scratch.
  Handle<JSFunction> CreateEmptyFunction();
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
  // Installs the contents of the native .js files on the global objects.
  // Used for creating a context from scratch.
  void InstallNativeFunctions();
  bool InstallNatives();
  void InstallCustomCallGenerators();
  void InstallJSFunctionResultCaches();
  // Used both for deserialized and from-scratch contexts to add the extensions
  // provided.
  static bool InstallExtensions(Handle<Context> global_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallExtension(const char* name);
  static bool InstallExtension(v8::RegisteredExtension* current);
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
  Handle<DescriptorArray> ComputeFunctionInstanceDescriptor(
      PrototypePropertyMode prototypeMode);
  void MakeFunctionInstancePrototypeWritable();

  static bool CompileBuiltin(int index);
  static bool CompileNative(Vector<const char> name, Handle<String> source);
  static bool CompileScriptCached(Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context);

  Handle<Context> result_;
  Handle<JSFunction> empty_function_;
  BootstrapperActive active_;
  friend class Bootstrapper;
};


void Bootstrapper::Iterate(ObjectVisitor* v) {
  extensions_cache.Iterate(v);
  v->Synchronize("Extensions");
}


Handle<Context> Bootstrapper::CreateEnvironment(
    Handle<Object> global_object,
    v8::Handle<v8::ObjectTemplate> global_template,
    v8::ExtensionConfiguration* extensions) {
  HandleScope scope;
  Handle<Context> env;
  Genesis genesis(global_object, global_template, extensions);
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
  Handle<Map> old_to_map = Handle<Map>(object->map());
  Handle<Map> new_to_map = Factory::CopyMapDropTransitions(old_to_map);
  new_to_map->set_prototype(*proto);
  object->set_map(*new_to_map);
}


void Bootstrapper::DetachGlobal(Handle<Context> env) {
  JSGlobalProxy::cast(env->global_proxy())->set_context(*Factory::null_value());
  SetObjectPrototype(Handle<JSObject>(env->global_proxy()),
                     Factory::null_value());
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
  Handle<String> symbol = Factory::LookupAsciiSymbol(name);
  Handle<Code> call_code = Handle<Code>(Builtins::builtin(call));
  Handle<JSFunction> function = prototype.is_null() ?
    Factory::NewFunctionWithoutPrototype(symbol, call_code) :
    Factory::NewFunctionWithPrototype(symbol,
                                      type,
                                      instance_size,
                                      prototype,
                                      call_code,
                                      is_ecma_native);
  SetProperty(target, symbol, function, DONT_ENUM);
  if (is_ecma_native) {
    function->shared()->set_instance_class_name(*symbol);
  }
  return function;
}


Handle<DescriptorArray> Genesis::ComputeFunctionInstanceDescriptor(
    PrototypePropertyMode prototypeMode) {
  Handle<DescriptorArray> result = Factory::empty_descriptor_array();

  if (prototypeMode != DONT_ADD_PROTOTYPE) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(
        DONT_ENUM |
        DONT_DELETE |
        (prototypeMode == ADD_READONLY_PROTOTYPE ? READ_ONLY : 0));
    result =
        Factory::CopyAppendProxyDescriptor(
            result,
            Factory::prototype_symbol(),
            Factory::NewProxy(&Accessors::FunctionPrototype),
            attributes);
  }

  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  // Add length.
  result =
      Factory::CopyAppendProxyDescriptor(
          result,
          Factory::length_symbol(),
          Factory::NewProxy(&Accessors::FunctionLength),
          attributes);

  // Add name.
  result =
      Factory::CopyAppendProxyDescriptor(
          result,
          Factory::name_symbol(),
          Factory::NewProxy(&Accessors::FunctionName),
          attributes);

  // Add arguments.
  result =
      Factory::CopyAppendProxyDescriptor(
          result,
          Factory::arguments_symbol(),
          Factory::NewProxy(&Accessors::FunctionArguments),
          attributes);

  // Add caller.
  result =
      Factory::CopyAppendProxyDescriptor(
          result,
          Factory::caller_symbol(),
          Factory::NewProxy(&Accessors::FunctionCaller),
          attributes);

  return result;
}


Handle<JSFunction> Genesis::CreateEmptyFunction() {
  // Allocate the map for function instances.
  Handle<Map> fm = Factory::NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  global_context()->set_function_instance_map(*fm);
  // Please note that the prototype property for function instances must be
  // writable.
  Handle<DescriptorArray> function_map_descriptors =
      ComputeFunctionInstanceDescriptor(ADD_WRITEABLE_PROTOTYPE);
  fm->set_instance_descriptors(*function_map_descriptors);
  fm->set_function_with_prototype(true);

  // Functions with this map will not have a 'prototype' property, and
  // can not be used as constructors.
  Handle<Map> function_without_prototype_map =
      Factory::NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  global_context()->set_function_without_prototype_map(
      *function_without_prototype_map);
  Handle<DescriptorArray> function_without_prototype_map_descriptors =
      ComputeFunctionInstanceDescriptor(DONT_ADD_PROTOTYPE);
  function_without_prototype_map->set_instance_descriptors(
      *function_without_prototype_map_descriptors);
  function_without_prototype_map->set_function_with_prototype(false);

  // Allocate the function map first and then patch the prototype later
  fm = Factory::NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  global_context()->set_function_map(*fm);
  function_map_descriptors =
      ComputeFunctionInstanceDescriptor(ADD_READONLY_PROTOTYPE);
  fm->set_instance_descriptors(*function_map_descriptors);
  fm->set_function_with_prototype(true);

  Handle<String> object_name = Handle<String>(Heap::Object_symbol());

  {  // --- O b j e c t ---
    Handle<JSFunction> object_fun =
        Factory::NewFunction(object_name, Factory::null_value());
    Handle<Map> object_function_map =
        Factory::NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    object_fun->set_initial_map(*object_function_map);
    object_function_map->set_constructor(*object_fun);

    global_context()->set_object_function(*object_fun);

    // Allocate a new prototype for the object function.
    Handle<JSObject> prototype = Factory::NewJSObject(Top::object_function(),
                                                      TENURED);

    global_context()->set_initial_object_prototype(*prototype);
    SetPrototype(object_fun, prototype);
    object_function_map->
      set_instance_descriptors(Heap::empty_descriptor_array());
  }

  // Allocate the empty function as the prototype for function ECMAScript
  // 262 15.3.4.
  Handle<String> symbol = Factory::LookupAsciiSymbol("Empty");
  Handle<JSFunction> empty_function =
      Factory::NewFunctionWithoutPrototype(symbol);

  // --- E m p t y ---
  Handle<Code> code =
      Handle<Code>(Builtins::builtin(Builtins::EmptyFunction));
  empty_function->set_code(*code);
  empty_function->shared()->set_code(*code);
  Handle<String> source = Factory::NewStringFromAscii(CStrVector("() {}"));
  Handle<Script> script = Factory::NewScript(source);
  script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
  empty_function->shared()->set_script(*script);
  empty_function->shared()->set_start_position(0);
  empty_function->shared()->set_end_position(source->length());
  empty_function->shared()->DontAdaptArguments();
  global_context()->function_map()->set_prototype(*empty_function);
  global_context()->function_instance_map()->set_prototype(*empty_function);
  global_context()->function_without_prototype_map()->
      set_prototype(*empty_function);

  // Allocate the function map first and then patch the prototype later
  Handle<Map> empty_fm = Factory::CopyMapDropDescriptors(
      function_without_prototype_map);
  empty_fm->set_instance_descriptors(
      *function_without_prototype_map_descriptors);
  empty_fm->set_prototype(global_context()->object_function()->prototype());
  empty_function->set_map(*empty_fm);
  return empty_function;
}


void Genesis::CreateRoots() {
  // Allocate the global context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // global context).
  global_context_ =
      Handle<Context>::cast(
          GlobalHandles::Create(*Factory::NewGlobalContext()));
  Top::set_context(*global_context());

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
    Handle<String> name = Handle<String>(Heap::empty_symbol());
    Handle<Code> code = Handle<Code>(Builtins::builtin(Builtins::Illegal));
    js_global_function =
        Factory::NewFunction(name, JS_GLOBAL_OBJECT_TYPE,
                             JSGlobalObject::kSize, code, true);
    // Change the constructor property of the prototype of the
    // hidden global function to refer to the Object function.
    Handle<JSObject> prototype =
        Handle<JSObject>(
            JSObject::cast(js_global_function->instance_prototype()));
    SetProperty(prototype, Factory::constructor_symbol(),
                Top::object_function(), NONE);
  } else {
    Handle<FunctionTemplateInfo> js_global_constructor(
        FunctionTemplateInfo::cast(js_global_template->constructor()));
    js_global_function =
        Factory::CreateApiFunction(js_global_constructor,
                                   Factory::InnerGlobalObject);
  }

  js_global_function->initial_map()->set_is_hidden_prototype();
  Handle<GlobalObject> inner_global =
      Factory::NewGlobalObject(js_global_function);
  if (inner_global_out != NULL) {
    *inner_global_out = inner_global;
  }

  // Step 2: create or re-initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_template.IsEmpty()) {
    Handle<String> name = Handle<String>(Heap::empty_symbol());
    Handle<Code> code = Handle<Code>(Builtins::builtin(Builtins::Illegal));
    global_proxy_function =
        Factory::NewFunction(name, JS_GLOBAL_PROXY_TYPE,
                             JSGlobalProxy::kSize, code, true);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_template);
    Handle<FunctionTemplateInfo> global_constructor(
            FunctionTemplateInfo::cast(data->constructor()));
    global_proxy_function =
        Factory::CreateApiFunction(global_constructor,
                                   Factory::OuterGlobalObject);
  }

  Handle<String> global_name = Factory::LookupAsciiSymbol("global");
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
        Factory::NewJSObject(global_proxy_function, TENURED));
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
                   Factory::LookupAsciiSymbol("global"),
                   inner_global,
                   attributes);
  // Setup the reference from the global object to the builtins object.
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
  global_context()->set_closure(*empty_function);
  global_context()->set_fcontext(*global_context());
  global_context()->set_previous(NULL);
  // Set extension and global object.
  global_context()->set_extension(*inner_global);
  global_context()->set_global(*inner_global);
  // Security setup: Set the security token of the global object to
  // its the inner global. This makes the security check between two
  // different contexts fail by default even in case of global
  // object reinitialization.
  global_context()->set_security_token(*inner_global);

  Handle<String> object_name = Handle<String>(Heap::Object_symbol());
  SetProperty(inner_global, object_name, Top::object_function(), DONT_ENUM);

  Handle<JSObject> global = Handle<JSObject>(global_context()->global());

  // Install global Function object
  InstallFunction(global, "Function", JS_FUNCTION_TYPE, JSFunction::kSize,
                  empty_function, Builtins::Illegal, true);  // ECMA native.

  {  // --- A r r a y ---
    Handle<JSFunction> array_function =
        InstallFunction(global, "Array", JS_ARRAY_TYPE, JSArray::kSize,
                        Top::initial_object_prototype(), Builtins::ArrayCode,
                        true);
    array_function->shared()->set_construct_stub(
        Builtins::builtin(Builtins::ArrayConstructCode));
    array_function->shared()->DontAdaptArguments();

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared()->set_length(1);
    Handle<DescriptorArray> array_descriptors =
        Factory::CopyAppendProxyDescriptor(
            Factory::empty_descriptor_array(),
            Factory::length_symbol(),
            Factory::NewProxy(&Accessors::ArrayLength),
            static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE));

    // Cache the fast JavaScript array map
    global_context()->set_js_array_map(array_function->initial_map());
    global_context()->js_array_map()->set_instance_descriptors(
        *array_descriptors);
    // array_function is used internally. JS code creating array object should
    // search for the 'Array' property on the global object and use that one
    // as the constructor. 'Array' property on a global object can be
    // overwritten by JS code.
    global_context()->set_array_function(*array_function);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun =
        InstallFunction(global, "Number", JS_VALUE_TYPE, JSValue::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        true);
    global_context()->set_number_function(*number_fun);
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun =
        InstallFunction(global, "Boolean", JS_VALUE_TYPE, JSValue::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        true);
    global_context()->set_boolean_function(*boolean_fun);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun =
        InstallFunction(global, "String", JS_VALUE_TYPE, JSValue::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        true);
    global_context()->set_string_function(*string_fun);
    // Add 'length' property to strings.
    Handle<DescriptorArray> string_descriptors =
        Factory::CopyAppendProxyDescriptor(
            Factory::empty_descriptor_array(),
            Factory::length_symbol(),
            Factory::NewProxy(&Accessors::StringLength),
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
        InstallFunction(global, "Date", JS_VALUE_TYPE, JSValue::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        true);

    global_context()->set_date_function(*date_fun);
  }


  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun =
        InstallFunction(global, "RegExp", JS_REGEXP_TYPE, JSRegExp::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        true);
    global_context()->set_regexp_function(*regexp_fun);

    ASSERT(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map());

    ASSERT_EQ(0, initial_map->inobject_properties());

    Handle<DescriptorArray> descriptors = Factory::NewDescriptorArray(5);
    PropertyAttributes final =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    int enum_index = 0;
    {
      // ECMA-262, section 15.10.7.1.
      FieldDescriptor field(Heap::source_symbol(),
                            JSRegExp::kSourceFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(0, &field);
    }
    {
      // ECMA-262, section 15.10.7.2.
      FieldDescriptor field(Heap::global_symbol(),
                            JSRegExp::kGlobalFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(1, &field);
    }
    {
      // ECMA-262, section 15.10.7.3.
      FieldDescriptor field(Heap::ignore_case_symbol(),
                            JSRegExp::kIgnoreCaseFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(2, &field);
    }
    {
      // ECMA-262, section 15.10.7.4.
      FieldDescriptor field(Heap::multiline_symbol(),
                            JSRegExp::kMultilineFieldIndex,
                            final,
                            enum_index++);
      descriptors->Set(3, &field);
    }
    {
      // ECMA-262, section 15.10.7.5.
      PropertyAttributes writable =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
      FieldDescriptor field(Heap::last_index_symbol(),
                            JSRegExp::kLastIndexFieldIndex,
                            writable,
                            enum_index++);
      descriptors->Set(4, &field);
    }
    descriptors->SetNextEnumerationIndex(enum_index);
    descriptors->Sort();

    initial_map->set_inobject_properties(5);
    initial_map->set_pre_allocated_property_fields(5);
    initial_map->set_unused_property_fields(0);
    initial_map->set_instance_size(
        initial_map->instance_size() + 5 * kPointerSize);
    initial_map->set_instance_descriptors(*descriptors);
    initial_map->set_visitor_id(StaticVisitorBase::GetVisitorId(*initial_map));
  }

  {  // -- J S O N
    Handle<String> name = Factory::NewStringFromAscii(CStrVector("JSON"));
    Handle<JSFunction> cons = Factory::NewFunction(
        name,
        Factory::the_hole_value());
    cons->SetInstancePrototype(global_context()->initial_object_prototype());
    cons->SetInstanceClassName(*name);
    Handle<JSObject> json_object = Factory::NewJSObject(cons, TENURED);
    ASSERT(json_object->IsJSObject());
    SetProperty(global, name, json_object, DONT_ENUM);
    global_context()->set_json_object(*json_object);
  }

  {  // --- arguments_boilerplate_
    // Make sure we can recognize argument objects at runtime.
    // This is done by introducing an anonymous function with
    // class_name equals 'Arguments'.
    Handle<String> symbol = Factory::LookupAsciiSymbol("Arguments");
    Handle<Code> code = Handle<Code>(Builtins::builtin(Builtins::Illegal));
    Handle<JSObject> prototype =
        Handle<JSObject>(
            JSObject::cast(global_context()->object_function()->prototype()));

    Handle<JSFunction> function =
        Factory::NewFunctionWithPrototype(symbol,
                                          JS_OBJECT_TYPE,
                                          JSObject::kHeaderSize,
                                          prototype,
                                          code,
                                          false);
    ASSERT(!function->has_initial_map());
    function->shared()->set_instance_class_name(*symbol);
    function->shared()->set_expected_nof_properties(2);
    Handle<JSObject> result = Factory::NewJSObject(function);

    global_context()->set_arguments_boilerplate(*result);
    // Note: callee must be added as the first property and
    //       length must be added as the second property.
    SetProperty(result, Factory::callee_symbol(),
                Factory::undefined_value(),
                DONT_ENUM);
    SetProperty(result, Factory::length_symbol(),
                Factory::undefined_value(),
                DONT_ENUM);

#ifdef DEBUG
    LookupResult lookup;
    result->LocalLookup(Heap::callee_symbol(), &lookup);
    ASSERT(lookup.IsProperty() && (lookup.type() == FIELD));
    ASSERT(lookup.GetFieldIndex() == Heap::arguments_callee_index);

    result->LocalLookup(Heap::length_symbol(), &lookup);
    ASSERT(lookup.IsProperty() && (lookup.type() == FIELD));
    ASSERT(lookup.GetFieldIndex() == Heap::arguments_length_index);

    ASSERT(result->map()->inobject_properties() > Heap::arguments_callee_index);
    ASSERT(result->map()->inobject_properties() > Heap::arguments_length_index);

    // Check the state of the object.
    ASSERT(result->HasFastProperties());
    ASSERT(result->HasFastElements());
#endif
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<Code> code = Handle<Code>(Builtins::builtin(Builtins::Illegal));
    Handle<JSFunction> context_extension_fun =
        Factory::NewFunction(Factory::empty_symbol(),
                             JS_CONTEXT_EXTENSION_OBJECT_TYPE,
                             JSObject::kHeaderSize,
                             code,
                             true);

    Handle<String> name = Factory::LookupAsciiSymbol("context_extension");
    context_extension_fun->shared()->set_instance_class_name(*name);
    global_context()->set_context_extension_function(*context_extension_fun);
  }


  {
    // Setup the call-as-function delegate.
    Handle<Code> code =
        Handle<Code>(Builtins::builtin(Builtins::HandleApiCallAsFunction));
    Handle<JSFunction> delegate =
        Factory::NewFunction(Factory::empty_symbol(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    global_context()->set_call_as_function_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  {
    // Setup the call-as-constructor delegate.
    Handle<Code> code =
        Handle<Code>(Builtins::builtin(Builtins::HandleApiCallAsConstructor));
    Handle<JSFunction> delegate =
        Factory::NewFunction(Factory::empty_symbol(), JS_OBJECT_TYPE,
                             JSObject::kHeaderSize, code, true);
    global_context()->set_call_as_constructor_delegate(*delegate);
    delegate->shared()->DontAdaptArguments();
  }

  // Initialize the out of memory slot.
  global_context()->set_out_of_memory(Heap::false_value());

  // Initialize the data slot.
  global_context()->set_data(Heap::undefined_value());
}


bool Genesis::CompileBuiltin(int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code = Bootstrapper::NativesSourceLookup(index);
  return CompileNative(name, source_code);
}


bool Genesis::CompileNative(Vector<const char> name, Handle<String> source) {
  HandleScope scope;
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debugger::set_compiling_natives(true);
#endif
  bool result = CompileScriptCached(name,
                                    source,
                                    NULL,
                                    NULL,
                                    Handle<Context>(Top::context()),
                                    true);
  ASSERT(Top::has_pending_exception() != result);
  if (!result) Top::clear_pending_exception();
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debugger::set_compiling_natives(false);
#endif
  return result;
}


bool Genesis::CompileScriptCached(Vector<const char> name,
                                  Handle<String> source,
                                  SourceCodeCache* cache,
                                  v8::Extension* extension,
                                  Handle<Context> top_context,
                                  bool use_runtime_context) {
  HandleScope scope;
  Handle<SharedFunctionInfo> function_info;

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  if (cache == NULL || !cache->Lookup(name, &function_info)) {
    ASSERT(source->IsAsciiRepresentation());
    Handle<String> script_name = Factory::NewStringFromUtf8(name);
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

  // Setup the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  ASSERT(top_context->IsGlobalContext());
  Handle<Context> context =
      Handle<Context>(use_runtime_context
                      ? Handle<Context>(top_context->runtime_context())
                      : top_context);
  Handle<JSFunction> fun =
      Factory::NewFunctionFromSharedFunctionInfo(function_info, context);

  // Call function using either the runtime object or the global
  // object as the receiver. Provide no parameters.
  Handle<Object> receiver =
      Handle<Object>(use_runtime_context
                     ? top_context->builtins()
                     : top_context->global());
  bool has_pending_exception;
  Handle<Object> result =
      Execution::Call(fun, receiver, 0, NULL, &has_pending_exception);
  if (has_pending_exception) return false;
  return true;
}


#define INSTALL_NATIVE(Type, name, var)                                  \
  Handle<String> var##_name = Factory::LookupAsciiSymbol(name);          \
  global_context()->set_##var(Type::cast(global_context()->              \
                                           builtins()->                  \
                                             GetProperty(*var##_name)));

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
  INSTALL_NATIVE(JSFunction, "MakeMessage", make_message_fun);
  INSTALL_NATIVE(JSFunction, "GetStackTraceLine", get_stack_trace_line_fun);
  INSTALL_NATIVE(JSObject, "functionCache", function_cache);
}

#undef INSTALL_NATIVE


bool Genesis::InstallNatives() {
  HandleScope scope;

  // Create a function for the builtins object. Allocate space for the
  // JavaScript builtins, a reference to the builtins object
  // (itself) and a reference to the global_context directly in the object.
  Handle<Code> code = Handle<Code>(Builtins::builtin(Builtins::Illegal));
  Handle<JSFunction> builtins_fun =
      Factory::NewFunction(Factory::empty_symbol(), JS_BUILTINS_OBJECT_TYPE,
                           JSBuiltinsObject::kSize, code, true);

  Handle<String> name = Factory::LookupAsciiSymbol("builtins");
  builtins_fun->shared()->set_instance_class_name(*name);

  // Allocate the builtins object.
  Handle<JSBuiltinsObject> builtins =
      Handle<JSBuiltinsObject>::cast(Factory::NewGlobalObject(builtins_fun));
  builtins->set_builtins(*builtins);
  builtins->set_global_context(*global_context());
  builtins->set_global_receiver(*builtins);

  // Setup the 'global' properties of the builtins object. The
  // 'global' property that refers to the global object is the only
  // way to get from code running in the builtins context to the
  // global object.
  static const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  SetProperty(builtins, Factory::LookupAsciiSymbol("global"),
              Handle<Object>(global_context()->global()), attributes);

  // Setup the reference from the global object to the builtins object.
  JSGlobalObject::cast(global_context()->global())->set_builtins(*builtins);

  // Create a bridge function that has context in the global context.
  Handle<JSFunction> bridge =
      Factory::NewFunction(Factory::empty_symbol(), Factory::undefined_value());
  ASSERT(bridge->context() == *Top::global_context());

  // Allocate the builtins context.
  Handle<Context> context =
    Factory::NewFunctionContext(Context::MIN_CONTEXT_SLOTS, bridge);
  context->set_global(*builtins);  // override builtins global object

  global_context()->set_runtime_context(*context);

  {  // -- S c r i p t
    // Builtin functions for Script.
    Handle<JSFunction> script_fun =
        InstallFunction(builtins, "Script", JS_VALUE_TYPE, JSValue::kSize,
                        Top::initial_object_prototype(), Builtins::Illegal,
                        false);
    Handle<JSObject> prototype =
        Factory::NewJSObject(Top::object_function(), TENURED);
    SetPrototype(script_fun, prototype);
    global_context()->set_script_function(*script_fun);

    // Add 'source' and 'data' property to scripts.
    PropertyAttributes common_attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
    Handle<Proxy> proxy_source = Factory::NewProxy(&Accessors::ScriptSource);
    Handle<DescriptorArray> script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            Factory::empty_descriptor_array(),
            Factory::LookupAsciiSymbol("source"),
            proxy_source,
            common_attributes);
    Handle<Proxy> proxy_name = Factory::NewProxy(&Accessors::ScriptName);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("name"),
            proxy_name,
            common_attributes);
    Handle<Proxy> proxy_id = Factory::NewProxy(&Accessors::ScriptId);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("id"),
            proxy_id,
            common_attributes);
    Handle<Proxy> proxy_line_offset =
        Factory::NewProxy(&Accessors::ScriptLineOffset);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("line_offset"),
            proxy_line_offset,
            common_attributes);
    Handle<Proxy> proxy_column_offset =
        Factory::NewProxy(&Accessors::ScriptColumnOffset);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("column_offset"),
            proxy_column_offset,
            common_attributes);
    Handle<Proxy> proxy_data = Factory::NewProxy(&Accessors::ScriptData);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("data"),
            proxy_data,
            common_attributes);
    Handle<Proxy> proxy_type = Factory::NewProxy(&Accessors::ScriptType);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("type"),
            proxy_type,
            common_attributes);
    Handle<Proxy> proxy_compilation_type =
        Factory::NewProxy(&Accessors::ScriptCompilationType);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("compilation_type"),
            proxy_compilation_type,
            common_attributes);
    Handle<Proxy> proxy_line_ends =
        Factory::NewProxy(&Accessors::ScriptLineEnds);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("line_ends"),
            proxy_line_ends,
            common_attributes);
    Handle<Proxy> proxy_context_data =
        Factory::NewProxy(&Accessors::ScriptContextData);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("context_data"),
            proxy_context_data,
            common_attributes);
    Handle<Proxy> proxy_eval_from_script =
        Factory::NewProxy(&Accessors::ScriptEvalFromScript);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("eval_from_script"),
            proxy_eval_from_script,
            common_attributes);
    Handle<Proxy> proxy_eval_from_script_position =
        Factory::NewProxy(&Accessors::ScriptEvalFromScriptPosition);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("eval_from_script_position"),
            proxy_eval_from_script_position,
            common_attributes);
    Handle<Proxy> proxy_eval_from_function_name =
        Factory::NewProxy(&Accessors::ScriptEvalFromFunctionName);
    script_descriptors =
        Factory::CopyAppendProxyDescriptor(
            script_descriptors,
            Factory::LookupAsciiSymbol("eval_from_function_name"),
            proxy_eval_from_function_name,
            common_attributes);

    Handle<Map> script_map = Handle<Map>(script_fun->initial_map());
    script_map->set_instance_descriptors(*script_descriptors);

    // Allocate the empty script.
    Handle<Script> script = Factory::NewScript(Factory::empty_string());
    script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
    Heap::public_set_empty_script(*script);
  }
  {
    // Builtin function for OpaqueReference -- a JSValue-based object,
    // that keeps its field isolated from JavaScript code. It may store
    // objects, that JavaScript code may not access.
    Handle<JSFunction> opaque_reference_fun =
        InstallFunction(builtins, "OpaqueReference", JS_VALUE_TYPE,
                        JSValue::kSize, Top::initial_object_prototype(),
                        Builtins::Illegal, false);
    Handle<JSObject> prototype =
        Factory::NewJSObject(Top::object_function(), TENURED);
    SetPrototype(opaque_reference_fun, prototype);
    global_context()->set_opaque_reference_function(*opaque_reference_fun);
  }

  if (FLAG_disable_native_files) {
    PrintF("Warning: Running without installed natives!\n");
    return true;
  }

  // Install natives.
  for (int i = Natives::GetDebuggerCount();
       i < Natives::GetBuiltinsCount();
       i++) {
    Vector<const char> name = Natives::GetScriptName(i);
    if (!CompileBuiltin(i)) return false;
    // TODO(ager): We really only need to install the JS builtin
    // functions on the builtins object after compiling and running
    // runtime.js.
    if (!InstallJSBuiltins(builtins)) return false;
  }

  InstallNativeFunctions();

  // Store the map for the string prototype after the natives has been compiled
  // and the String function has been setup.
  Handle<JSFunction> string_function(global_context()->string_function());
  ASSERT(JSObject::cast(
      string_function->initial_map()->prototype())->HasFastProperties());
  global_context()->set_string_function_prototype_map(
      HeapObject::cast(string_function->initial_map()->prototype())->map());

  InstallCustomCallGenerators();

  // Install Function.prototype.call and apply.
  { Handle<String> key = Factory::function_class_symbol();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(GetProperty(Top::global(), key));
    Handle<JSObject> proto =
        Handle<JSObject>(JSObject::cast(function->instance_prototype()));

    // Install the call and the apply functions.
    Handle<JSFunction> call =
        InstallFunction(proto, "call", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::FunctionCall,
                        false);
    Handle<JSFunction> apply =
        InstallFunction(proto, "apply", JS_OBJECT_TYPE, JSObject::kHeaderSize,
                        Handle<JSObject>::null(),
                        Builtins::FunctionApply,
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
        Factory::NewMap(JS_ARRAY_TYPE, JSRegExpResult::kSize);
    initial_map->set_constructor(*array_constructor);

    // Set prototype on map.
    initial_map->set_non_instance_prototype(false);
    initial_map->set_prototype(*array_prototype);

    // Update map with length accessor from Array and add "index" and "input".
    Handle<Map> array_map(global_context()->js_array_map());
    Handle<DescriptorArray> array_descriptors(
        array_map->instance_descriptors());
    ASSERT_EQ(1, array_descriptors->number_of_descriptors());

    Handle<DescriptorArray> reresult_descriptors =
        Factory::NewDescriptorArray(3);

    reresult_descriptors->CopyFrom(0, *array_descriptors, 0);

    int enum_index = 0;
    {
      FieldDescriptor index_field(Heap::index_symbol(),
                                  JSRegExpResult::kIndexIndex,
                                  NONE,
                                  enum_index++);
      reresult_descriptors->Set(1, &index_field);
    }

    {
      FieldDescriptor input_field(Heap::input_symbol(),
                                  JSRegExpResult::kInputIndex,
                                  NONE,
                                  enum_index++);
      reresult_descriptors->Set(2, &input_field);
    }
    reresult_descriptors->Sort();

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


static void InstallCustomCallGenerator(Handle<JSFunction> holder_function,
                                       const char* function_name,
                                       int id) {
  Handle<JSObject> proto(JSObject::cast(holder_function->instance_prototype()));
  Handle<String> name = Factory::LookupAsciiSymbol(function_name);
  Handle<JSFunction> function(JSFunction::cast(proto->GetProperty(*name)));
  function->shared()->set_function_data(Smi::FromInt(id));
}


void Genesis::InstallCustomCallGenerators() {
  HandleScope scope;
#define INSTALL_CALL_GENERATOR(holder_fun, fun_name, name)                \
  {                                                                       \
    Handle<JSFunction> holder(global_context()->holder_fun##_function()); \
    const int id = CallStubCompiler::k##name##CallGenerator;              \
    InstallCustomCallGenerator(holder, #fun_name, id);                    \
  }
  CUSTOM_CALL_IC_GENERATORS(INSTALL_CALL_GENERATOR)
#undef INSTALL_CALL_GENERATOR
}


// Do not forget to update macros.py with named constant
// of cache id.
#define JSFUNCTION_RESULT_CACHE_LIST(F) \
  F(16, global_context()->regexp_function())


static FixedArray* CreateCache(int size, JSFunction* factory) {
  // Caches are supposed to live for a long time, allocate in old space.
  int array_size = JSFunctionResultCache::kEntriesIndex + 2 * size;
  // Cannot use cast as object is not fully initialized yet.
  JSFunctionResultCache* cache = reinterpret_cast<JSFunctionResultCache*>(
      *Factory::NewFixedArrayWithHoles(array_size, TENURED));
  cache->set(JSFunctionResultCache::kFactoryIndex, factory);
  cache->MakeZeroSize();
  return cache;
}


void Genesis::InstallJSFunctionResultCaches() {
  const int kNumberOfCaches = 0 +
#define F(size, func) + 1
    JSFUNCTION_RESULT_CACHE_LIST(F)
#undef F
  ;

  Handle<FixedArray> caches = Factory::NewFixedArray(kNumberOfCaches, TENURED);

  int index = 0;
#define F(size, func) caches->set(index++, CreateCache(size, func));
    JSFUNCTION_RESULT_CACHE_LIST(F)
#undef F

  global_context()->set_jsfunction_result_caches(*caches);
}


int BootstrapperActive::nesting_ = 0;


bool Bootstrapper::InstallExtensions(Handle<Context> global_context,
                                     v8::ExtensionConfiguration* extensions) {
  BootstrapperActive active;
  SaveContext saved_context;
  Top::set_context(*global_context);
  if (!Genesis::InstallExtensions(global_context, extensions)) return false;
  Genesis::InstallSpecialObjects(global_context);
  return true;
}


void Genesis::InstallSpecialObjects(Handle<Context> global_context) {
  HandleScope scope;
  Handle<JSGlobalObject> js_global(
      JSGlobalObject::cast(global_context->global()));
  // Expose the natives in global if a name for it is specified.
  if (FLAG_expose_natives_as != NULL && strlen(FLAG_expose_natives_as) != 0) {
    Handle<String> natives_string =
        Factory::LookupAsciiSymbol(FLAG_expose_natives_as);
    SetProperty(js_global, natives_string,
                Handle<JSObject>(js_global->builtins()), DONT_ENUM);
  }

  Handle<Object> Error = GetProperty(js_global, "Error");
  if (Error->IsJSObject()) {
    Handle<String> name = Factory::LookupAsciiSymbol("stackTraceLimit");
    SetProperty(Handle<JSObject>::cast(Error),
                name,
                Handle<Smi>(Smi::FromInt(FLAG_stack_trace_limit)),
                NONE);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Expose the debug global object in global if a name for it is specified.
  if (FLAG_expose_debug_as != NULL && strlen(FLAG_expose_debug_as) != 0) {
    // If loading fails we just bail out without installing the
    // debugger but without tanking the whole context.
    if (!Debug::Load()) return;
    // Set the security token for the debugger context to the same as
    // the shell global context to allow calling between these (otherwise
    // exposing debug global object doesn't make much sense).
    Debug::debug_context()->set_security_token(
        global_context->security_token());

    Handle<String> debug_string =
        Factory::LookupAsciiSymbol(FLAG_expose_debug_as);
    SetProperty(js_global, debug_string,
        Handle<Object>(Debug::debug_context()->global_proxy()), DONT_ENUM);
  }
#endif
}


bool Genesis::InstallExtensions(Handle<Context> global_context,
                                v8::ExtensionConfiguration* extensions) {
  // Clear coloring of extension list
  v8::RegisteredExtension* current = v8::RegisteredExtension::first_extension();
  while (current != NULL) {
    current->set_state(v8::UNVISITED);
    current = current->next();
  }
  // Install auto extensions.
  current = v8::RegisteredExtension::first_extension();
  while (current != NULL) {
    if (current->extension()->auto_enable())
      InstallExtension(current);
    current = current->next();
  }

  if (FLAG_expose_gc) InstallExtension("v8/gc");
  if (FLAG_expose_externalize_string) InstallExtension("v8/externalize");

  if (extensions == NULL) return true;
  // Install required extensions
  int count = v8::ImplementationUtilities::GetNameCount(extensions);
  const char** names = v8::ImplementationUtilities::GetNames(extensions);
  for (int i = 0; i < count; i++) {
    if (!InstallExtension(names[i]))
      return false;
  }

  return true;
}


// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(const char* name) {
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
  return InstallExtension(current);
}


bool Genesis::InstallExtension(v8::RegisteredExtension* current) {
  HandleScope scope;

  if (current->state() == v8::INSTALLED) return true;
  // The current node has already been visited so there must be a
  // cycle in the dependency graph; fail.
  if (current->state() == v8::VISITED) {
    v8::Utils::ReportApiFailure(
        "v8::Context::New()", "Circular extension dependency");
    return false;
  }
  ASSERT(current->state() == v8::UNVISITED);
  current->set_state(v8::VISITED);
  v8::Extension* extension = current->extension();
  // Install the extension's dependencies
  for (int i = 0; i < extension->dependency_count(); i++) {
    if (!InstallExtension(extension->dependencies()[i])) return false;
  }
  Vector<const char> source = CStrVector(extension->source());
  Handle<String> source_code = Factory::NewStringFromAscii(source);
  bool result = CompileScriptCached(CStrVector(extension->name()),
                                    source_code,
                                    &extensions_cache,
                                    extension,
                                    Handle<Context>(Top::context()),
                                    false);
  ASSERT(Top::has_pending_exception() != result);
  if (!result) {
    Top::clear_pending_exception();
  }
  current->set_state(v8::INSTALLED);
  return result;
}


bool Genesis::InstallJSBuiltins(Handle<JSBuiltinsObject> builtins) {
  HandleScope scope;
  for (int i = 0; i < Builtins::NumberOfJavaScriptBuiltins(); i++) {
    Builtins::JavaScript id = static_cast<Builtins::JavaScript>(i);
    Handle<String> name = Factory::LookupAsciiSymbol(Builtins::GetName(id));
    Handle<JSFunction> function
        = Handle<JSFunction>(JSFunction::cast(builtins->GetProperty(*name)));
    builtins->set_javascript_builtin(id, *function);
    Handle<SharedFunctionInfo> shared
        = Handle<SharedFunctionInfo>(function->shared());
    if (!EnsureCompiled(shared, CLEAR_EXCEPTION)) return false;
    // Set the code object on the function object.
    function->set_code(function->shared()->code());
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
    ASSERT(Top::has_pending_exception());
    Top::clear_pending_exception();
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
      PropertyDetails details = PropertyDetails(descs->GetDetails(i));
      switch (details.type()) {
        case FIELD: {
          HandleScope inner;
          Handle<String> key = Handle<String>(descs->GetKey(i));
          int index = descs->GetFieldIndex(i);
          Handle<Object> value = Handle<Object>(from->FastPropertyAt(index));
          SetProperty(to, key, value, details.attributes());
          break;
        }
        case CONSTANT_FUNCTION: {
          HandleScope inner;
          Handle<String> key = Handle<String>(descs->GetKey(i));
          Handle<JSFunction> fun =
              Handle<JSFunction>(descs->GetConstantFunction(i));
          SetProperty(to, key, fun, details.attributes());
          break;
        }
        case CALLBACKS: {
          LookupResult result;
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
          SetNormalizedProperty(to, key, callbacks, d);
          break;
        }
        case MAP_TRANSITION:
        case CONSTANT_TRANSITION:
        case NULL_DESCRIPTOR:
          // Ignore non-properties.
          break;
        case NORMAL:
          // Do not occur since the from object has fast properties.
        case INTERCEPTOR:
          // No element in instance descriptors have interceptor type.
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
        LookupResult result;
        to->LocalLookup(String::cast(raw_key), &result);
        if (result.IsProperty()) continue;
        // Set the property.
        Handle<String> key = Handle<String>(String::cast(raw_key));
        Handle<Object> value = Handle<Object>(properties->ValueAt(i));
        if (value->IsJSGlobalPropertyCell()) {
          value = Handle<Object>(JSGlobalPropertyCell::cast(*value)->value());
        }
        PropertyDetails details = properties->DetailsAt(i);
        SetProperty(to, key, value, details.attributes());
      }
    }
  }
}


void Genesis::TransferIndexedProperties(Handle<JSObject> from,
                                        Handle<JSObject> to) {
  // Cloning the elements array is sufficient.
  Handle<FixedArray> from_elements =
      Handle<FixedArray>(FixedArray::cast(from->elements()));
  Handle<FixedArray> to_elements = Factory::CopyFixedArray(from_elements);
  to->set_elements(*to_elements);
}


void Genesis::TransferObject(Handle<JSObject> from, Handle<JSObject> to) {
  HandleScope outer;

  ASSERT(!from->IsJSArray());
  ASSERT(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<Map> old_to_map = Handle<Map>(to->map());
  Handle<Map> new_to_map = Factory::CopyMapDropTransitions(old_to_map);
  new_to_map->set_prototype(from->map()->prototype());
  to->set_map(*new_to_map);
}


void Genesis::MakeFunctionInstancePrototypeWritable() {
  // Make a new function map so all future functions
  // will have settable and enumerable prototype properties.
  HandleScope scope;

  Handle<DescriptorArray> function_map_descriptors =
      ComputeFunctionInstanceDescriptor(ADD_WRITEABLE_PROTOTYPE);
  Handle<Map> fm = Factory::CopyMapDropDescriptors(Top::function_map());
  fm->set_instance_descriptors(*function_map_descriptors);
  fm->set_function_with_prototype(true);
  Top::context()->global_context()->set_function_map(*fm);
}


Genesis::Genesis(Handle<Object> global_object,
                 v8::Handle<v8::ObjectTemplate> global_template,
                 v8::ExtensionConfiguration* extensions) {
  result_ = Handle<Context>::null();
  // If V8 isn't running and cannot be initialized, just return.
  if (!V8::IsRunning() && !V8::Initialize(NULL)) return;

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  HandleScope scope;
  SaveContext saved_context;

  Handle<Context> new_context = Snapshot::NewContextFromSnapshot();
  if (!new_context.is_null()) {
    global_context_ =
      Handle<Context>::cast(GlobalHandles::Create(*new_context));
    Top::set_context(*global_context_);
    i::Counters::contexts_created_by_snapshot.Increment();
    result_ = global_context_;
    JSFunction* empty_function =
        JSFunction::cast(result_->function_map()->prototype());
    empty_function_ = Handle<JSFunction>(empty_function);
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
    Handle<JSFunction> empty_function = CreateEmptyFunction();
    Handle<GlobalObject> inner_global;
    Handle<JSGlobalProxy> global_proxy =
        CreateNewGlobals(global_template, global_object, &inner_global);
    HookUpGlobalProxy(inner_global, global_proxy);
    InitializeGlobal(inner_global, empty_function);
    InstallJSFunctionResultCaches();
    if (!InstallNatives()) return;

    MakeFunctionInstancePrototypeWritable();

    if (!ConfigureGlobalObjects(global_template)) return;
    i::Counters::contexts_created_from_scratch.Increment();
  }

  result_ = global_context_;
}


// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() {
  return BootstrapperActive::ArchiveSpacePerThread();
}


// Archive statics that are thread local.
char* Bootstrapper::ArchiveState(char* to) {
  return BootstrapperActive::ArchiveState(to);
}


// Restore statics that are thread local.
char* Bootstrapper::RestoreState(char* from) {
  return BootstrapperActive::RestoreState(from);
}


// Called when the top-level V8 mutex is destroyed.
void Bootstrapper::FreeThreadResources() {
  ASSERT(!BootstrapperActive::IsActive());
}


// Reserve space for statics needing saving and restoring.
int BootstrapperActive::ArchiveSpacePerThread() {
  return sizeof(nesting_);
}


// Archive statics that are thread local.
char* BootstrapperActive::ArchiveState(char* to) {
  *reinterpret_cast<int*>(to) = nesting_;
  nesting_ = 0;
  return to + sizeof(nesting_);
}


// Restore statics that are thread local.
char* BootstrapperActive::RestoreState(char* from) {
  nesting_ = *reinterpret_cast<int*>(from);
  return from + sizeof(nesting_);
}

} }  // namespace v8::internal

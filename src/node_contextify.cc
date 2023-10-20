// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_contextify.h"

#include "base_object-inl.h"
#include "memory_tracker-inl.h"
#include "module_wrap.h"
#include "node_context_data.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_snapshot_builder.h"
#include "node_watchdog.h"
#include "util-inl.h"

namespace node {
namespace contextify {

using errors::TryCatchScope;

using v8::Array;
using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::IndexedPropertyHandlerConfiguration;
using v8::Int32;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::MeasureMemoryExecution;
using v8::MeasureMemoryMode;
using v8::MicrotaskQueue;
using v8::MicrotasksPolicy;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::PrimitiveArray;
using v8::Promise;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::PropertyDescriptor;
using v8::PropertyHandlerFlags;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Symbol;
using v8::Uint32;
using v8::UnboundScript;
using v8::Value;
using v8::WeakCallbackInfo;

// The vm module executes code in a sandboxed environment with a different
// global object than the rest of the code. This is achieved by applying
// every call that changes or queries a property on the global `this` in the
// sandboxed code, to the sandbox object.
//
// The implementation uses V8's interceptors for methods like `set`, `get`,
// `delete`, `defineProperty`, and for any query of the property attributes.
// Property handlers with interceptors are set on the object template for
// the sandboxed code. Handlers for both named properties and for indexed
// properties are used. Their functionality is almost identical, the indexed
// interceptors mostly just call the named interceptors.
//
// For every `get` of a global property in the sandboxed context, the
// interceptor callback checks the sandbox object for the property.
// If the property is defined on the sandbox, that result is returned to
// the original call instead of finishing the query on the global object.
//
// For every `set` of a global property, the interceptor callback defines or
// changes the property both on the sandbox and the global proxy.

namespace {

// Convert an int to a V8 Name (String or Symbol).
Local<Name> Uint32ToName(Local<Context> context, uint32_t index) {
  return Uint32::New(context->GetIsolate(), index)->ToString(context)
      .ToLocalChecked();
}

}  // anonymous namespace

BaseObjectPtr<ContextifyContext> ContextifyContext::New(
    Environment* env, Local<Object> sandbox_obj, ContextOptions* options) {
  HandleScope scope(env->isolate());
  Local<ObjectTemplate> object_template = env->contextify_global_template();
  DCHECK(!object_template.IsEmpty());
  const SnapshotData* snapshot_data = env->isolate_data()->snapshot_data();

  MicrotaskQueue* queue =
      options->own_microtask_queue
          ? options->own_microtask_queue.get()
          : env->isolate()->GetCurrentContext()->GetMicrotaskQueue();

  Local<Context> v8_context;
  if (!(CreateV8Context(env->isolate(), object_template, snapshot_data, queue)
            .ToLocal(&v8_context))) {
    // Allocation failure, maximum call stack size reached, termination, etc.
    return BaseObjectPtr<ContextifyContext>();
  }
  return New(v8_context, env, sandbox_obj, options);
}

void ContextifyContext::MemoryInfo(MemoryTracker* tracker) const {}

ContextifyContext::ContextifyContext(Environment* env,
                                     Local<Object> wrapper,
                                     Local<Context> v8_context,
                                     ContextOptions* options)
    : BaseObject(env, wrapper),
      microtask_queue_(options->own_microtask_queue
                           ? options->own_microtask_queue.release()
                           : nullptr) {
  context_.Reset(env->isolate(), v8_context);
  // This should only be done after the initial initializations of the context
  // global object is finished.
  DCHECK_NULL(v8_context->GetAlignedPointerFromEmbedderData(
      ContextEmbedderIndex::kContextifyContext));
  v8_context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kContextifyContext, this);
  // It's okay to make this reference weak - V8 would create an internal
  // reference to this context via the constructor of the wrapper.
  // As long as the wrapper is alive, it's constructor is alive, and so
  // is the context.
  context_.SetWeak();
}

ContextifyContext::~ContextifyContext() {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);

  env()->UnassignFromContext(PersistentToLocal::Weak(isolate, context_));
  context_.Reset();
}

void ContextifyContext::InitializeGlobalTemplates(IsolateData* isolate_data) {
  DCHECK(isolate_data->contextify_wrapper_template().IsEmpty());
  Local<FunctionTemplate> global_func_template =
      FunctionTemplate::New(isolate_data->isolate());
  Local<ObjectTemplate> global_object_template =
      global_func_template->InstanceTemplate();

  NamedPropertyHandlerConfiguration config(
      PropertyGetterCallback,
      PropertySetterCallback,
      PropertyDescriptorCallback,
      PropertyDeleterCallback,
      PropertyEnumeratorCallback,
      PropertyDefinerCallback,
      {},
      PropertyHandlerFlags::kHasNoSideEffect);

  IndexedPropertyHandlerConfiguration indexed_config(
      IndexedPropertyGetterCallback,
      IndexedPropertySetterCallback,
      IndexedPropertyDescriptorCallback,
      IndexedPropertyDeleterCallback,
      PropertyEnumeratorCallback,
      IndexedPropertyDefinerCallback,
      {},
      PropertyHandlerFlags::kHasNoSideEffect);

  global_object_template->SetHandler(config);
  global_object_template->SetHandler(indexed_config);
  isolate_data->set_contextify_global_template(global_object_template);

  Local<FunctionTemplate> wrapper_func_template =
      BaseObject::MakeLazilyInitializedJSTemplate(isolate_data);
  Local<ObjectTemplate> wrapper_object_template =
      wrapper_func_template->InstanceTemplate();
  isolate_data->set_contextify_wrapper_template(wrapper_object_template);
}

MaybeLocal<Context> ContextifyContext::CreateV8Context(
    Isolate* isolate,
    Local<ObjectTemplate> object_template,
    const SnapshotData* snapshot_data,
    MicrotaskQueue* queue) {
  EscapableHandleScope scope(isolate);

  Local<Context> ctx;
  if (snapshot_data == nullptr) {
    ctx = Context::New(isolate,
                       nullptr,  // extensions
                       object_template,
                       {},  // global object
                       {},  // deserialization callback
                       queue);
    if (ctx.IsEmpty() || InitializeBaseContextForSnapshot(ctx).IsNothing()) {
      return MaybeLocal<Context>();
    }
  } else if (!Context::FromSnapshot(isolate,
                                    SnapshotData::kNodeVMContextIndex,
                                    {},       // deserialization callback
                                    nullptr,  // extensions
                                    {},       // global object
                                    queue)
                  .ToLocal(&ctx)) {
    return MaybeLocal<Context>();
  }

  return scope.Escape(ctx);
}

BaseObjectPtr<ContextifyContext> ContextifyContext::New(
    Local<Context> v8_context,
    Environment* env,
    Local<Object> sandbox_obj,
    ContextOptions* options) {
  HandleScope scope(env->isolate());
  // This only initializes part of the context. The primordials are
  // only initialized when needed because even deserializing them slows
  // things down significantly and they are only needed in rare occasions
  // in the vm contexts.
  if (InitializeContextRuntime(v8_context).IsNothing()) {
    return BaseObjectPtr<ContextifyContext>();
  }

  Local<Context> main_context = env->context();
  Local<Object> new_context_global = v8_context->Global();
  v8_context->SetSecurityToken(main_context->GetSecurityToken());

  // We need to tie the lifetime of the sandbox object with the lifetime of
  // newly created context. We do this by making them hold references to each
  // other. The context can directly hold a reference to the sandbox as an
  // embedder data field. The sandbox uses a private symbol to hold a reference
  // to the ContextifyContext wrapper which in turn internally references
  // the context from its constructor.
  v8_context->SetEmbedderData(ContextEmbedderIndex::kSandboxObject,
                              sandbox_obj);

  // Delegate the code generation validation to
  // node::ModifyCodeGenerationFromStrings.
  v8_context->AllowCodeGenerationFromStrings(false);
  v8_context->SetEmbedderData(
      ContextEmbedderIndex::kAllowCodeGenerationFromStrings,
      options->allow_code_gen_strings);
  v8_context->SetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration,
                              options->allow_code_gen_wasm);

  Utf8Value name_val(env->isolate(), options->name);
  ContextInfo info(*name_val);
  if (!options->origin.IsEmpty()) {
    Utf8Value origin_val(env->isolate(), options->origin);
    info.origin = *origin_val;
  }

  BaseObjectPtr<ContextifyContext> result;
  Local<Object> wrapper;
  {
    Context::Scope context_scope(v8_context);
    Local<String> ctor_name = sandbox_obj->GetConstructorName();
    if (!ctor_name->Equals(v8_context, env->object_string()).FromMaybe(false) &&
        new_context_global
            ->DefineOwnProperty(
                v8_context,
                v8::Symbol::GetToStringTag(env->isolate()),
                ctor_name,
                static_cast<v8::PropertyAttribute>(v8::DontEnum))
            .IsNothing()) {
      return BaseObjectPtr<ContextifyContext>();
    }
    env->AssignToContext(v8_context, nullptr, info);

    if (!env->contextify_wrapper_template()
             ->NewInstance(v8_context)
             .ToLocal(&wrapper)) {
      return BaseObjectPtr<ContextifyContext>();
    }

    result =
        MakeBaseObject<ContextifyContext>(env, wrapper, v8_context, options);
    // The only strong reference to the wrapper will come from the sandbox.
    result->MakeWeak();
  }

  if (sandbox_obj
          ->SetPrivate(
              v8_context, env->contextify_context_private_symbol(), wrapper)
          .IsNothing()) {
    return BaseObjectPtr<ContextifyContext>();
  }

  return result;
}

void ContextifyContext::CreatePerIsolateProperties(
    IsolateData* isolate_data, Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethod(isolate, target, "makeContext", MakeContext);
  SetMethod(isolate, target, "isContext", IsContext);
  SetMethod(isolate, target, "compileFunction", CompileFunction);
  SetMethod(isolate, target, "containsModuleSyntax", ContainsModuleSyntax);
}

void ContextifyContext::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(MakeContext);
  registry->Register(IsContext);
  registry->Register(CompileFunction);
  registry->Register(ContainsModuleSyntax);
  registry->Register(PropertyGetterCallback);
  registry->Register(PropertySetterCallback);
  registry->Register(PropertyDescriptorCallback);
  registry->Register(PropertyDeleterCallback);
  registry->Register(PropertyEnumeratorCallback);
  registry->Register(PropertyDefinerCallback);
  registry->Register(IndexedPropertyGetterCallback);
  registry->Register(IndexedPropertySetterCallback);
  registry->Register(IndexedPropertyDescriptorCallback);
  registry->Register(IndexedPropertyDeleterCallback);
  registry->Register(IndexedPropertyDefinerCallback);
}

// makeContext(sandbox, name, origin, strings, wasm);
void ContextifyContext::MakeContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 6);
  CHECK(args[0]->IsObject());
  Local<Object> sandbox = args[0].As<Object>();

  // Don't allow contextifying a sandbox multiple times.
  CHECK(
      !sandbox->HasPrivate(
          env->context(),
          env->contextify_context_private_symbol()).FromJust());

  ContextOptions options;

  CHECK(args[1]->IsString());
  options.name = args[1].As<String>();

  CHECK(args[2]->IsString() || args[2]->IsUndefined());
  if (args[2]->IsString()) {
    options.origin = args[2].As<String>();
  }

  CHECK(args[3]->IsBoolean());
  options.allow_code_gen_strings = args[3].As<Boolean>();

  CHECK(args[4]->IsBoolean());
  options.allow_code_gen_wasm = args[4].As<Boolean>();

  if (args[5]->IsBoolean() && args[5]->BooleanValue(env->isolate())) {
    options.own_microtask_queue =
        MicrotaskQueue::New(env->isolate(), MicrotasksPolicy::kExplicit);
  }

  TryCatchScope try_catch(env);
  BaseObjectPtr<ContextifyContext> context_ptr =
      ContextifyContext::New(env, sandbox, &options);

  if (try_catch.HasCaught()) {
    if (!try_catch.HasTerminated())
      try_catch.ReThrow();
    return;
  }
}


void ContextifyContext::IsContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  Local<Object> sandbox = args[0].As<Object>();

  Maybe<bool> result =
      sandbox->HasPrivate(env->context(),
                          env->contextify_context_private_symbol());
  args.GetReturnValue().Set(result.FromJust());
}


void ContextifyContext::WeakCallback(
    const WeakCallbackInfo<ContextifyContext>& data) {
  ContextifyContext* context = data.GetParameter();
  delete context;
}

// static
ContextifyContext* ContextifyContext::ContextFromContextifiedSandbox(
    Environment* env,
    const Local<Object>& sandbox) {
  Local<Value> context_global;
  if (sandbox
          ->GetPrivate(env->context(), env->contextify_context_private_symbol())
          .ToLocal(&context_global) &&
      context_global->IsObject()) {
    return Unwrap<ContextifyContext>(context_global.As<Object>());
  }
  return nullptr;
}

template <typename T>
ContextifyContext* ContextifyContext::Get(const PropertyCallbackInfo<T>& args) {
  return Get(args.This());
}

ContextifyContext* ContextifyContext::Get(Local<Object> object) {
  Local<Context> context;
  if (!object->GetCreationContext().ToLocal(&context)) {
    return nullptr;
  }
  if (!ContextEmbedderTag::IsNodeContext(context)) {
    return nullptr;
  }
  return static_cast<ContextifyContext*>(
      context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kContextifyContext));
}

bool ContextifyContext::IsStillInitializing(const ContextifyContext* ctx) {
  return ctx == nullptr || ctx->context_.IsEmpty();
}

// static
void ContextifyContext::PropertyGetterCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Local<Context> context = ctx->context();
  Local<Object> sandbox = ctx->sandbox();
  MaybeLocal<Value> maybe_rv =
      sandbox->GetRealNamedProperty(context, property);
  if (maybe_rv.IsEmpty()) {
    maybe_rv =
        ctx->global_proxy()->GetRealNamedProperty(context, property);
  }

  Local<Value> rv;
  if (maybe_rv.ToLocal(&rv)) {
    if (rv == sandbox)
      rv = ctx->global_proxy();

    args.GetReturnValue().Set(rv);
  }
}

// static
void ContextifyContext::PropertySetterCallback(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Local<Context> context = ctx->context();
  PropertyAttribute attributes = PropertyAttribute::None;
  bool is_declared_on_global_proxy = ctx->global_proxy()
      ->GetRealNamedPropertyAttributes(context, property)
      .To(&attributes);
  bool read_only =
      static_cast<int>(attributes) &
      static_cast<int>(PropertyAttribute::ReadOnly);

  bool is_declared_on_sandbox = ctx->sandbox()
      ->GetRealNamedPropertyAttributes(context, property)
      .To(&attributes);
  read_only = read_only ||
      (static_cast<int>(attributes) &
      static_cast<int>(PropertyAttribute::ReadOnly));

  if (read_only)
    return;

  // true for x = 5
  // false for this.x = 5
  // false for Object.defineProperty(this, 'foo', ...)
  // false for vmResult.x = 5 where vmResult = vm.runInContext();
  bool is_contextual_store = ctx->global_proxy() != args.This();

  // Indicator to not return before setting (undeclared) function declarations
  // on the sandbox in strict mode, i.e. args.ShouldThrowOnError() = true.
  // True for 'function f() {}', 'this.f = function() {}',
  // 'var f = function()'.
  // In effect only for 'function f() {}' because
  // var f = function(), is_declared = true
  // this.f = function() {}, is_contextual_store = false.
  bool is_function = value->IsFunction();

  bool is_declared = is_declared_on_global_proxy || is_declared_on_sandbox;
  if (!is_declared && args.ShouldThrowOnError() && is_contextual_store &&
      !is_function)
    return;

  if (!is_declared && property->IsSymbol()) return;
  if (ctx->sandbox()->Set(context, property, value).IsNothing()) return;

  Local<Value> desc;
  if (is_declared_on_sandbox &&
      ctx->sandbox()
          ->GetOwnPropertyDescriptor(context, property)
          .ToLocal(&desc) &&
      !desc->IsUndefined()) {
    Environment* env = Environment::GetCurrent(context);
    Local<Object> desc_obj = desc.As<Object>();

    // We have to specify the return value for any contextual or get/set
    // property
    if (desc_obj->HasOwnProperty(context, env->get_string()).FromMaybe(false) ||
        desc_obj->HasOwnProperty(context, env->set_string()).FromMaybe(false))
      args.GetReturnValue().Set(value);
  }
}

// static
void ContextifyContext::PropertyDescriptorCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Local<Context> context = ctx->context();

  Local<Object> sandbox = ctx->sandbox();

  if (sandbox->HasOwnProperty(context, property).FromMaybe(false)) {
    Local<Value> desc;
    if (sandbox->GetOwnPropertyDescriptor(context, property).ToLocal(&desc)) {
      args.GetReturnValue().Set(desc);
    }
  }
}

// static
void ContextifyContext::PropertyDefinerCallback(
    Local<Name> property,
    const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Local<Context> context = ctx->context();
  Isolate* isolate = context->GetIsolate();

  PropertyAttribute attributes = PropertyAttribute::None;
  bool is_declared =
      ctx->global_proxy()->GetRealNamedPropertyAttributes(context,
                                                          property)
          .To(&attributes);
  bool read_only =
      static_cast<int>(attributes) &
          static_cast<int>(PropertyAttribute::ReadOnly);

  // If the property is set on the global as read_only, don't change it on
  // the global or sandbox.
  if (is_declared && read_only)
    return;

  Local<Object> sandbox = ctx->sandbox();

  auto define_prop_on_sandbox =
      [&] (PropertyDescriptor* desc_for_sandbox) {
        if (desc.has_enumerable()) {
          desc_for_sandbox->set_enumerable(desc.enumerable());
        }
        if (desc.has_configurable()) {
          desc_for_sandbox->set_configurable(desc.configurable());
        }
        // Set the property on the sandbox.
        USE(sandbox->DefineProperty(context, property, *desc_for_sandbox));
      };

  if (desc.has_get() || desc.has_set()) {
    PropertyDescriptor desc_for_sandbox(
        desc.has_get() ? desc.get() : Undefined(isolate).As<Value>(),
        desc.has_set() ? desc.set() : Undefined(isolate).As<Value>());

    define_prop_on_sandbox(&desc_for_sandbox);
  } else {
    Local<Value> value =
        desc.has_value() ? desc.value() : Undefined(isolate).As<Value>();

    if (desc.has_writable()) {
      PropertyDescriptor desc_for_sandbox(value, desc.writable());
      define_prop_on_sandbox(&desc_for_sandbox);
    } else {
      PropertyDescriptor desc_for_sandbox(value);
      define_prop_on_sandbox(&desc_for_sandbox);
    }
  }
}

// static
void ContextifyContext::PropertyDeleterCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Boolean>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Maybe<bool> success = ctx->sandbox()->Delete(ctx->context(), property);

  if (success.FromMaybe(false))
    return;

  // Delete failed on the sandbox, intercept and do not delete on
  // the global object.
  args.GetReturnValue().Set(false);
}

// static
void ContextifyContext::PropertyEnumeratorCallback(
    const PropertyCallbackInfo<Array>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Local<Array> properties;

  if (!ctx->sandbox()->GetPropertyNames(ctx->context()).ToLocal(&properties))
    return;

  args.GetReturnValue().Set(properties);
}

// static
void ContextifyContext::IndexedPropertyGetterCallback(
    uint32_t index,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  ContextifyContext::PropertyGetterCallback(
      Uint32ToName(ctx->context(), index), args);
}


void ContextifyContext::IndexedPropertySetterCallback(
    uint32_t index,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  ContextifyContext::PropertySetterCallback(
      Uint32ToName(ctx->context(), index), value, args);
}

// static
void ContextifyContext::IndexedPropertyDescriptorCallback(
    uint32_t index,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  ContextifyContext::PropertyDescriptorCallback(
      Uint32ToName(ctx->context(), index), args);
}


void ContextifyContext::IndexedPropertyDefinerCallback(
    uint32_t index,
    const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  ContextifyContext::PropertyDefinerCallback(
      Uint32ToName(ctx->context(), index), desc, args);
}

// static
void ContextifyContext::IndexedPropertyDeleterCallback(
    uint32_t index,
    const PropertyCallbackInfo<Boolean>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (IsStillInitializing(ctx)) return;

  Maybe<bool> success = ctx->sandbox()->Delete(ctx->context(), index);

  if (success.FromMaybe(false))
    return;

  // Delete failed on the sandbox, intercept and do not delete on
  // the global object.
  args.GetReturnValue().Set(false);
}

void ContextifyScript::CreatePerIsolateProperties(
    IsolateData* isolate_data, Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  Local<String> class_name = FIXED_ONE_BYTE_STRING(isolate, "ContextifyScript");

  Local<FunctionTemplate> script_tmpl = NewFunctionTemplate(isolate, New);
  script_tmpl->InstanceTemplate()->SetInternalFieldCount(
      ContextifyScript::kInternalFieldCount);
  script_tmpl->SetClassName(class_name);
  SetProtoMethod(isolate, script_tmpl, "createCachedData", CreateCachedData);
  SetProtoMethod(isolate, script_tmpl, "runInContext", RunInContext);

  target->Set(isolate, "ContextifyScript", script_tmpl);
  isolate_data->set_script_context_constructor_template(script_tmpl);
}

void ContextifyScript::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(CreateCachedData);
  registry->Register(RunInContext);
}

void ContextifyScript::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  CHECK(args.IsConstructCall());

  const int argc = args.Length();
  CHECK_GE(argc, 2);

  CHECK(args[0]->IsString());
  Local<String> code = args[0].As<String>();

  CHECK(args[1]->IsString());
  Local<String> filename = args[1].As<String>();

  int line_offset = 0;
  int column_offset = 0;
  Local<ArrayBufferView> cached_data_buf;
  bool produce_cached_data = false;
  Local<Context> parsing_context = context;

  Local<Symbol> id_symbol;
  if (argc > 2) {
    // new ContextifyScript(code, filename, lineOffset, columnOffset,
    //                      cachedData, produceCachedData, parsingContext,
    //                      hostDefinedOptionId)
    CHECK_EQ(argc, 8);
    CHECK(args[2]->IsNumber());
    line_offset = args[2].As<Int32>()->Value();
    CHECK(args[3]->IsNumber());
    column_offset = args[3].As<Int32>()->Value();
    if (!args[4]->IsUndefined()) {
      CHECK(args[4]->IsArrayBufferView());
      cached_data_buf = args[4].As<ArrayBufferView>();
    }
    CHECK(args[5]->IsBoolean());
    produce_cached_data = args[5]->IsTrue();
    if (!args[6]->IsUndefined()) {
      CHECK(args[6]->IsObject());
      ContextifyContext* sandbox =
          ContextifyContext::ContextFromContextifiedSandbox(
              env, args[6].As<Object>());
      CHECK_NOT_NULL(sandbox);
      parsing_context = sandbox->context();
    }
    CHECK(args[7]->IsSymbol());
    id_symbol = args[7].As<Symbol>();
  }

  ContextifyScript* contextify_script =
      new ContextifyScript(env, args.This());

  if (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACING_CATEGORY_NODE2(vm, script)) != 0) {
    Utf8Value fn(isolate, filename);
    TRACE_EVENT_BEGIN1(TRACING_CATEGORY_NODE2(vm, script),
                       "ContextifyScript::New",
                       "filename",
                       TRACE_STR_COPY(*fn));
  }

  ScriptCompiler::CachedData* cached_data = nullptr;
  if (!cached_data_buf.IsEmpty()) {
    uint8_t* data = static_cast<uint8_t*>(cached_data_buf->Buffer()->Data());
    cached_data = new ScriptCompiler::CachedData(
        data + cached_data_buf->ByteOffset(), cached_data_buf->ByteLength());
  }

  Local<PrimitiveArray> host_defined_options =
      PrimitiveArray::New(isolate, loader::HostDefinedOptions::kLength);
  host_defined_options->Set(
      isolate, loader::HostDefinedOptions::kID, id_symbol);

  ScriptOrigin origin(isolate,
                      filename,
                      line_offset,                          // line offset
                      column_offset,                        // column offset
                      true,                                 // is cross origin
                      -1,                                   // script id
                      Local<Value>(),                       // source map URL
                      false,                                // is opaque (?)
                      false,                                // is WASM
                      false,                                // is ES Module
                      host_defined_options);
  ScriptCompiler::Source source(code, origin, cached_data);
  ScriptCompiler::CompileOptions compile_options =
      ScriptCompiler::kNoCompileOptions;

  if (source.GetCachedData() != nullptr)
    compile_options = ScriptCompiler::kConsumeCodeCache;

  TryCatchScope try_catch(env);
  ShouldNotAbortOnUncaughtScope no_abort_scope(env);
  Context::Scope scope(parsing_context);

  MaybeLocal<UnboundScript> maybe_v8_script =
      ScriptCompiler::CompileUnboundScript(isolate, &source, compile_options);

  Local<UnboundScript> v8_script;
  if (!maybe_v8_script.ToLocal(&v8_script)) {
    errors::DecorateErrorStack(env, try_catch);
    no_abort_scope.Close();
    if (!try_catch.HasTerminated())
      try_catch.ReThrow();
    TRACE_EVENT_END0(TRACING_CATEGORY_NODE2(vm, script),
                     "ContextifyScript::New");
    return;
  }

  contextify_script->script_.Reset(isolate, v8_script);
  contextify_script->script_.SetWeak();
  contextify_script->object()->SetInternalFieldForNodeCore(kUnboundScriptSlot,
                                                           v8_script);

  std::unique_ptr<ScriptCompiler::CachedData> new_cached_data;
  if (produce_cached_data) {
    new_cached_data.reset(ScriptCompiler::CreateCodeCache(v8_script));
  }

  if (contextify_script->object()
          ->SetPrivate(context, env->host_defined_option_symbol(), id_symbol)
          .IsNothing()) {
    return;
  }

  if (StoreCodeCacheResult(env,
                           args.This(),
                           compile_options,
                           source,
                           produce_cached_data,
                           std::move(new_cached_data))
          .IsNothing()) {
    return;
  }

  if (args.This()
          ->Set(env->context(),
                env->source_map_url_string(),
                v8_script->GetSourceMappingURL())
          .IsNothing())
    return;

  TRACE_EVENT_END0(TRACING_CATEGORY_NODE2(vm, script), "ContextifyScript::New");
}

Maybe<bool> StoreCodeCacheResult(
    Environment* env,
    Local<Object> target,
    ScriptCompiler::CompileOptions compile_options,
    const v8::ScriptCompiler::Source& source,
    bool produce_cached_data,
    std::unique_ptr<ScriptCompiler::CachedData> new_cached_data) {
  Local<Context> context;
  if (!target->GetCreationContext().ToLocal(&context)) {
    return Nothing<bool>();
  }
  if (compile_options == ScriptCompiler::kConsumeCodeCache) {
    if (target
            ->Set(
                context,
                env->cached_data_rejected_string(),
                Boolean::New(env->isolate(), source.GetCachedData()->rejected))
            .IsNothing()) {
      return Nothing<bool>();
    }
  }
  if (produce_cached_data) {
    bool cached_data_produced = new_cached_data != nullptr;
    if (cached_data_produced) {
      MaybeLocal<Object> buf =
          Buffer::Copy(env,
                       reinterpret_cast<const char*>(new_cached_data->data),
                       new_cached_data->length);
      if (target->Set(context, env->cached_data_string(), buf.ToLocalChecked())
              .IsNothing()) {
        return Nothing<bool>();
      }
    }
    if (target
            ->Set(context,
                  env->cached_data_produced_string(),
                  Boolean::New(env->isolate(), cached_data_produced))
            .IsNothing()) {
      return Nothing<bool>();
    }
  }
  return Just(true);
}

// TODO(RaisinTen): Reuse in ContextifyContext::CompileFunction().
MaybeLocal<Function> CompileFunction(Local<Context> context,
                                     Local<String> filename,
                                     Local<String> content,
                                     std::vector<Local<String>>* parameters) {
  ScriptOrigin script_origin(context->GetIsolate(), filename, 0, 0, true);
  ScriptCompiler::Source script_source(content, script_origin);

  return ScriptCompiler::CompileFunction(context,
                                         &script_source,
                                         parameters->size(),
                                         parameters->data(),
                                         0,
                                         nullptr);
}

bool ContextifyScript::InstanceOf(Environment* env,
                                  const Local<Value>& value) {
  return !value.IsEmpty() &&
         env->script_context_constructor_template()->HasInstance(value);
}

void ContextifyScript::CreateCachedData(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  ContextifyScript* wrapped_script;
  ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder());
  Local<UnboundScript> unbound_script =
      PersistentToLocal::Default(env->isolate(), wrapped_script->script_);
  std::unique_ptr<ScriptCompiler::CachedData> cached_data(
      ScriptCompiler::CreateCodeCache(unbound_script));
  if (!cached_data) {
    args.GetReturnValue().Set(Buffer::New(env, 0).ToLocalChecked());
  } else {
    MaybeLocal<Object> buf = Buffer::Copy(
        env,
        reinterpret_cast<const char*>(cached_data->data),
        cached_data->length);
    args.GetReturnValue().Set(buf.ToLocalChecked());
  }
}

void ContextifyScript::RunInContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ContextifyScript* wrapped_script;
  ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder());

  CHECK_EQ(args.Length(), 5);
  CHECK(args[0]->IsObject() || args[0]->IsNull());

  Local<Context> context;
  v8::MicrotaskQueue* microtask_queue = nullptr;

  if (args[0]->IsObject()) {
    Local<Object> sandbox = args[0].As<Object>();
    // Get the context from the sandbox
    ContextifyContext* contextify_context =
        ContextifyContext::ContextFromContextifiedSandbox(env, sandbox);
    CHECK_NOT_NULL(contextify_context);
    CHECK_EQ(contextify_context->env(), env);

    context = contextify_context->context();
    if (context.IsEmpty()) return;

    microtask_queue = contextify_context->microtask_queue();
  } else {
    context = env->context();
  }

  TRACE_EVENT0(TRACING_CATEGORY_NODE2(vm, script), "RunInContext");

  CHECK(args[1]->IsNumber());
  int64_t timeout = args[1]->IntegerValue(env->context()).FromJust();

  CHECK(args[2]->IsBoolean());
  bool display_errors = args[2]->IsTrue();

  CHECK(args[3]->IsBoolean());
  bool break_on_sigint = args[3]->IsTrue();

  CHECK(args[4]->IsBoolean());
  bool break_on_first_line = args[4]->IsTrue();

  // Do the eval within the context
  EvalMachine(context,
              env,
              timeout,
              display_errors,
              break_on_sigint,
              break_on_first_line,
              microtask_queue,
              args);
}

bool ContextifyScript::EvalMachine(Local<Context> context,
                                   Environment* env,
                                   const int64_t timeout,
                                   const bool display_errors,
                                   const bool break_on_sigint,
                                   const bool break_on_first_line,
                                   MicrotaskQueue* mtask_queue,
                                   const FunctionCallbackInfo<Value>& args) {
  Context::Scope context_scope(context);

  if (!env->can_call_into_js())
    return false;
  if (!ContextifyScript::InstanceOf(env, args.Holder())) {
    THROW_ERR_INVALID_THIS(
        env,
        "Script methods can only be called on script instances.");
    return false;
  }

  TryCatchScope try_catch(env);
  Isolate::SafeForTerminationScope safe_for_termination(env->isolate());
  ContextifyScript* wrapped_script;
  ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder(), false);
  Local<UnboundScript> unbound_script =
      PersistentToLocal::Default(env->isolate(), wrapped_script->script_);
  Local<Script> script = unbound_script->BindToCurrentContext();

#if HAVE_INSPECTOR
  if (break_on_first_line) {
    env->inspector_agent()->PauseOnNextJavascriptStatement("Break on start");
  }
#endif

  MaybeLocal<Value> result;
  bool timed_out = false;
  bool received_signal = false;
  auto run = [&]() {
    MaybeLocal<Value> result = script->Run(context);
    if (!result.IsEmpty() && mtask_queue != nullptr)
      mtask_queue->PerformCheckpoint(env->isolate());
    return result;
  };
  if (break_on_sigint && timeout != -1) {
    Watchdog wd(env->isolate(), timeout, &timed_out);
    SigintWatchdog swd(env->isolate(), &received_signal);
    result = run();
  } else if (break_on_sigint) {
    SigintWatchdog swd(env->isolate(), &received_signal);
    result = run();
  } else if (timeout != -1) {
    Watchdog wd(env->isolate(), timeout, &timed_out);
    result = run();
  } else {
    result = run();
  }

  // Convert the termination exception into a regular exception.
  if (timed_out || received_signal) {
    if (!env->is_main_thread() && env->is_stopping())
      return false;
    env->isolate()->CancelTerminateExecution();
    // It is possible that execution was terminated by another timeout in
    // which this timeout is nested, so check whether one of the watchdogs
    // from this invocation is responsible for termination.
    if (timed_out) {
      node::THROW_ERR_SCRIPT_EXECUTION_TIMEOUT(env, timeout);
    } else if (received_signal) {
      node::THROW_ERR_SCRIPT_EXECUTION_INTERRUPTED(env);
    }
  }

  if (try_catch.HasCaught()) {
    if (!timed_out && !received_signal && display_errors) {
      // We should decorate non-termination exceptions
      errors::DecorateErrorStack(env, try_catch);
    }

    // If there was an exception thrown during script execution, re-throw it.
    // If one of the above checks threw, re-throw the exception instead of
    // letting try_catch catch it.
    // If execution has been terminated, but not by one of the watchdogs from
    // this invocation, this will re-throw a `null` value.
    if (!try_catch.HasTerminated())
      try_catch.ReThrow();

    return false;
  }

  args.GetReturnValue().Set(result.ToLocalChecked());
  return true;
}

ContextifyScript::ContextifyScript(Environment* env, Local<Object> object)
    : BaseObject(env, object) {
  MakeWeak();
}

ContextifyScript::~ContextifyScript() {}

void ContextifyContext::CompileFunction(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  // Argument 1: source code
  CHECK(args[0]->IsString());
  Local<String> code = args[0].As<String>();

  // Argument 2: filename
  CHECK(args[1]->IsString());
  Local<String> filename = args[1].As<String>();

  // Argument 3: line offset
  CHECK(args[2]->IsNumber());
  int line_offset = args[2].As<Int32>()->Value();

  // Argument 4: column offset
  CHECK(args[3]->IsNumber());
  int column_offset = args[3].As<Int32>()->Value();

  // Argument 5: cached data (optional)
  Local<ArrayBufferView> cached_data_buf;
  if (!args[4]->IsUndefined()) {
    CHECK(args[4]->IsArrayBufferView());
    cached_data_buf = args[4].As<ArrayBufferView>();
  }

  // Argument 6: produce cache data
  CHECK(args[5]->IsBoolean());
  bool produce_cached_data = args[5]->IsTrue();

  // Argument 7: parsing context (optional)
  Local<Context> parsing_context;
  if (!args[6]->IsUndefined()) {
    CHECK(args[6]->IsObject());
    ContextifyContext* sandbox =
        ContextifyContext::ContextFromContextifiedSandbox(
            env, args[6].As<Object>());
    CHECK_NOT_NULL(sandbox);
    parsing_context = sandbox->context();
  } else {
    parsing_context = context;
  }

  // Argument 8: context extensions (optional)
  Local<Array> context_extensions_buf;
  if (!args[7]->IsUndefined()) {
    CHECK(args[7]->IsArray());
    context_extensions_buf = args[7].As<Array>();
  }

  // Argument 9: params for the function (optional)
  Local<Array> params_buf;
  if (!args[8]->IsUndefined()) {
    CHECK(args[8]->IsArray());
    params_buf = args[8].As<Array>();
  }

  // Argument 10: host-defined option symbol
  CHECK(args[9]->IsSymbol());
  Local<Symbol> id_symbol = args[9].As<Symbol>();

  // Read cache from cached data buffer
  ScriptCompiler::CachedData* cached_data = nullptr;
  if (!cached_data_buf.IsEmpty()) {
    uint8_t* data = static_cast<uint8_t*>(cached_data_buf->Buffer()->Data());
    cached_data = new ScriptCompiler::CachedData(
      data + cached_data_buf->ByteOffset(), cached_data_buf->ByteLength());
  }

  Local<PrimitiveArray> host_defined_options =
      GetHostDefinedOptions(isolate, id_symbol);
  ScriptCompiler::Source source =
      GetCommonJSSourceInstance(isolate,
                                code,
                                filename,
                                line_offset,
                                column_offset,
                                host_defined_options,
                                cached_data);
  ScriptCompiler::CompileOptions options = GetCompileOptions(source);

  Context::Scope scope(parsing_context);

  // Read context extensions from buffer
  std::vector<Local<Object>> context_extensions;
  if (!context_extensions_buf.IsEmpty()) {
    for (uint32_t n = 0; n < context_extensions_buf->Length(); n++) {
      Local<Value> val;
      if (!context_extensions_buf->Get(context, n).ToLocal(&val)) return;
      CHECK(val->IsObject());
      context_extensions.push_back(val.As<Object>());
    }
  }

  // Read params from params buffer
  std::vector<Local<String>> params;
  if (!params_buf.IsEmpty()) {
    for (uint32_t n = 0; n < params_buf->Length(); n++) {
      Local<Value> val;
      if (!params_buf->Get(context, n).ToLocal(&val)) return;
      CHECK(val->IsString());
      params.push_back(val.As<String>());
    }
  }

  TryCatchScope try_catch(env);
  Local<Object> result = CompileFunctionAndCacheResult(env,
                                                       parsing_context,
                                                       &source,
                                                       params,
                                                       context_extensions,
                                                       options,
                                                       produce_cached_data,
                                                       id_symbol,
                                                       try_catch);

  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return;
  }

  if (result.IsEmpty()) {
    return;
  }
  args.GetReturnValue().Set(result);
}

Local<PrimitiveArray> ContextifyContext::GetHostDefinedOptions(
    Isolate* isolate, Local<Symbol> id_symbol) {
  Local<PrimitiveArray> host_defined_options =
      PrimitiveArray::New(isolate, loader::HostDefinedOptions::kLength);
  host_defined_options->Set(
      isolate, loader::HostDefinedOptions::kID, id_symbol);
  return host_defined_options;
}

ScriptCompiler::Source ContextifyContext::GetCommonJSSourceInstance(
    Isolate* isolate,
    Local<String> code,
    Local<String> filename,
    int line_offset,
    int column_offset,
    Local<PrimitiveArray> host_defined_options,
    ScriptCompiler::CachedData* cached_data) {
  ScriptOrigin origin(isolate,
                      filename,
                      line_offset,     // line offset
                      column_offset,   // column offset
                      true,            // is cross origin
                      -1,              // script id
                      Local<Value>(),  // source map URL
                      false,           // is opaque (?)
                      false,           // is WASM
                      false,           // is ES Module
                      host_defined_options);
  return ScriptCompiler::Source(code, origin, cached_data);
}

ScriptCompiler::CompileOptions ContextifyContext::GetCompileOptions(
    const ScriptCompiler::Source& source) {
  ScriptCompiler::CompileOptions options;
  if (source.GetCachedData() != nullptr) {
    options = ScriptCompiler::kConsumeCodeCache;
  } else {
    options = ScriptCompiler::kNoCompileOptions;
  }
  return options;
}

Local<Object> ContextifyContext::CompileFunctionAndCacheResult(
    Environment* env,
    Local<Context> parsing_context,
    ScriptCompiler::Source* source,
    std::vector<Local<String>> params,
    std::vector<Local<Object>> context_extensions,
    ScriptCompiler::CompileOptions options,
    bool produce_cached_data,
    Local<Symbol> id_symbol,
    const TryCatchScope& try_catch) {
  MaybeLocal<Function> maybe_fn = ScriptCompiler::CompileFunction(
      parsing_context,
      source,
      params.size(),
      params.data(),
      context_extensions.size(),
      context_extensions.data(),
      options,
      v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason);

  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      errors::DecorateErrorStack(env, try_catch);
      return Object::New(env->isolate());
    }
  }

  Local<Context> context = env->context();
  if (fn->SetPrivate(context, env->host_defined_option_symbol(), id_symbol)
          .IsNothing()) {
    return Object::New(env->isolate());
  }

  Isolate* isolate = env->isolate();
  Local<Object> result = Object::New(isolate);
  if (result->Set(parsing_context, env->function_string(), fn).IsNothing())
    return Object::New(env->isolate());
  if (result
          ->Set(parsing_context,
                env->source_map_url_string(),
                fn->GetScriptOrigin().SourceMapUrl())
          .IsNothing())
    return Object::New(env->isolate());

  std::unique_ptr<ScriptCompiler::CachedData> new_cached_data;
  if (produce_cached_data) {
    new_cached_data.reset(ScriptCompiler::CreateCodeCacheForFunction(fn));
  }
  if (StoreCodeCacheResult(env,
                           result,
                           options,
                           *source,
                           produce_cached_data,
                           std::move(new_cached_data))
          .IsNothing()) {
    return Object::New(env->isolate());
  }

  return result;
}

// When compiling as CommonJS source code that contains ESM syntax, the
// following error messages are returned:
// - `import` statements: "Cannot use import statement outside a module"
// - `export` statements: "Unexpected token 'export'"
// - `import.meta` references: "Cannot use 'import.meta' outside a module"
// Dynamic `import()` is permitted in CommonJS, so it does not error.
// While top-level `await` is not permitted in CommonJS, it returns the same
// error message as when `await` is used in a sync function, so we don't use it
// as a disambiguation.
constexpr std::array<std::string_view, 3> esm_syntax_error_messages = {
    "Cannot use import statement outside a module",  // `import` statements
    "Unexpected token 'export'",                     // `export` statements
    "Cannot use 'import.meta' outside a module"};    // `import.meta` references

void ContextifyContext::ContainsModuleSyntax(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  if (args.Length() == 0) {
    return THROW_ERR_MISSING_ARGS(
        env, "containsModuleSyntax needs at least 1 argument");
  }

  // Argument 2: filename; if undefined, use empty string
  Local<String> filename = String::Empty(isolate);
  if (!args[1]->IsUndefined()) {
    CHECK(args[1]->IsString());
    filename = args[1].As<String>();
  }

  // Argument 1: source code; if undefined, read from filename in argument 2
  Local<String> code;
  if (args[0]->IsUndefined()) {
    CHECK(!filename.IsEmpty());
    const char* filename_str = Utf8Value(isolate, filename).out();
    std::string contents;
    int result = ReadFileSync(&contents, filename_str);
    if (result != 0) {
      isolate->ThrowException(
          ERR_MODULE_NOT_FOUND(isolate, "Cannot read file %s", filename_str));
      return;
    }
    code = String::NewFromUtf8(isolate,
                               contents.c_str(),
                               v8::NewStringType::kNormal,
                               contents.length())
               .ToLocalChecked();
  } else {
    CHECK(args[0]->IsString());
    code = args[0].As<String>();
  }

  // TODO(geoffreybooth): Centralize this rather than matching the logic in
  // cjs/loader.js and translators.js
  Local<String> script_id = String::Concat(
      isolate, String::NewFromUtf8(isolate, "cjs:").ToLocalChecked(), filename);
  Local<Symbol> id_symbol = Symbol::New(isolate, script_id);

  Local<PrimitiveArray> host_defined_options =
      GetHostDefinedOptions(isolate, id_symbol);
  ScriptCompiler::Source source = GetCommonJSSourceInstance(
      isolate, code, filename, 0, 0, host_defined_options, nullptr);
  ScriptCompiler::CompileOptions options = GetCompileOptions(source);

  std::vector<Local<String>> params = {
      String::NewFromUtf8(isolate, "exports").ToLocalChecked(),
      String::NewFromUtf8(isolate, "require").ToLocalChecked(),
      String::NewFromUtf8(isolate, "module").ToLocalChecked(),
      String::NewFromUtf8(isolate, "__filename").ToLocalChecked(),
      String::NewFromUtf8(isolate, "__dirname").ToLocalChecked()};

  TryCatchScope try_catch(env);

  ContextifyContext::CompileFunctionAndCacheResult(env,
                                                   context,
                                                   &source,
                                                   params,
                                                   std::vector<Local<Object>>(),
                                                   options,
                                                   true,
                                                   id_symbol,
                                                   try_catch);

  bool found_error_message_caused_by_module_syntax = false;
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    Utf8Value message_value(env->isolate(), try_catch.Message()->Get());
    auto message = message_value.ToStringView();

    for (const auto& error_message : esm_syntax_error_messages) {
      if (message.find(error_message) != std::string_view::npos) {
        found_error_message_caused_by_module_syntax = true;
        break;
      }
    }
  }
  args.GetReturnValue().Set(found_error_message_caused_by_module_syntax);
}

static void StartSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  int ret = SigintWatchdogHelper::GetInstance()->Start();
  args.GetReturnValue().Set(ret == 0);
}

static void StopSigintWatchdog(const FunctionCallbackInfo<Value>& args) {
  bool had_pending_signals = SigintWatchdogHelper::GetInstance()->Stop();
  args.GetReturnValue().Set(had_pending_signals);
}

static void WatchdogHasPendingSigint(const FunctionCallbackInfo<Value>& args) {
  bool ret = SigintWatchdogHelper::GetInstance()->HasPendingSignal();
  args.GetReturnValue().Set(ret);
}

static void MeasureMemory(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());
  int32_t mode = args[0].As<v8::Int32>()->Value();
  int32_t execution = args[1].As<v8::Int32>()->Value();
  Isolate* isolate = args.GetIsolate();

  Local<Context> current_context = isolate->GetCurrentContext();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(current_context).ToLocal(&resolver)) return;
  std::unique_ptr<v8::MeasureMemoryDelegate> delegate =
      v8::MeasureMemoryDelegate::Default(
          isolate,
          current_context,
          resolver,
          static_cast<v8::MeasureMemoryMode>(mode));
  isolate->MeasureMemory(std::move(delegate),
                         static_cast<v8::MeasureMemoryExecution>(execution));
  Local<Promise> promise = resolver->GetPromise();

  args.GetReturnValue().Set(promise);
}

void CreatePerIsolateProperties(IsolateData* isolate_data,
                                Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();

  ContextifyContext::CreatePerIsolateProperties(isolate_data, target);
  ContextifyScript::CreatePerIsolateProperties(isolate_data, target);

  SetMethod(isolate, target, "startSigintWatchdog", StartSigintWatchdog);
  SetMethod(isolate, target, "stopSigintWatchdog", StopSigintWatchdog);
  // Used in tests.
  SetMethodNoSideEffect(
      isolate, target, "watchdogHasPendingSigint", WatchdogHasPendingSigint);

  SetMethod(isolate, target, "measureMemory", MeasureMemory);
}

static void CreatePerContextProperties(Local<Object> target,
                                       Local<Value> unused,
                                       Local<Context> context,
                                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<Object> constants = Object::New(env->isolate());
  Local<Object> measure_memory = Object::New(env->isolate());
  Local<Object> memory_execution = Object::New(env->isolate());

  {
    Local<Object> memory_mode = Object::New(env->isolate());
    MeasureMemoryMode SUMMARY = MeasureMemoryMode::kSummary;
    MeasureMemoryMode DETAILED = MeasureMemoryMode::kDetailed;
    NODE_DEFINE_CONSTANT(memory_mode, SUMMARY);
    NODE_DEFINE_CONSTANT(memory_mode, DETAILED);
    READONLY_PROPERTY(measure_memory, "mode", memory_mode);
  }

  {
    MeasureMemoryExecution DEFAULT = MeasureMemoryExecution::kDefault;
    MeasureMemoryExecution EAGER = MeasureMemoryExecution::kEager;
    NODE_DEFINE_CONSTANT(memory_execution, DEFAULT);
    NODE_DEFINE_CONSTANT(memory_execution, EAGER);
    READONLY_PROPERTY(measure_memory, "execution", memory_execution);
  }

  READONLY_PROPERTY(constants, "measureMemory", measure_memory);

  target->Set(context, env->constants_string(), constants).Check();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  ContextifyContext::RegisterExternalReferences(registry);
  ContextifyScript::RegisterExternalReferences(registry);

  registry->Register(StartSigintWatchdog);
  registry->Register(StopSigintWatchdog);
  registry->Register(WatchdogHasPendingSigint);
  registry->Register(MeasureMemory);
}
}  // namespace contextify
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    contextify, node::contextify::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(contextify,
                              node::contextify::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(contextify,
                                node::contextify::RegisterExternalReferences)

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

#include "node_errors.h"
#include "node_internals.h"
#include "node_watchdog.h"
#include "base_object-inl.h"
#include "node_contextify.h"
#include "node_context_data.h"
#include "node_errors.h"
#include "module_wrap.h"

namespace node {
namespace contextify {

using errors::TryCatchScope;

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::IndexedPropertyHandlerConfiguration;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PrimitiveArray;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::PropertyDescriptor;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Symbol;
using v8::Uint32;
using v8::UnboundScript;
using v8::Value;
using v8::WeakCallbackInfo;
using v8::WeakCallbackType;

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

ContextifyContext::ContextifyContext(
    Environment* env,
    Local<Object> sandbox_obj, const ContextOptions& options) : env_(env) {
  MaybeLocal<Context> v8_context = CreateV8Context(env, sandbox_obj, options);

  // Allocation failure, maximum call stack size reached, termination, etc.
  if (v8_context.IsEmpty()) return;

  context_.Reset(env->isolate(), v8_context.ToLocalChecked());
  context_.SetWeak(this, WeakCallback, WeakCallbackType::kParameter);
}


// This is an object that just keeps an internal pointer to this
// ContextifyContext.  It's passed to the NamedPropertyHandler.  If we
// pass the main JavaScript context object we're embedded in, then the
// NamedPropertyHandler will store a reference to it forever and keep it
// from getting gc'd.
MaybeLocal<Object> ContextifyContext::CreateDataWrapper(Environment* env) {
  Local<Object> wrapper;
  if (!env->script_data_constructor_function()
           ->NewInstance(env->context())
           .ToLocal(&wrapper)) {
    return MaybeLocal<Object>();
  }

  wrapper->SetAlignedPointerInInternalField(0, this);
  return wrapper;
}

MaybeLocal<Context> ContextifyContext::CreateV8Context(
    Environment* env,
    Local<Object> sandbox_obj,
    const ContextOptions& options) {
  EscapableHandleScope scope(env->isolate());
  Local<FunctionTemplate> function_template =
      FunctionTemplate::New(env->isolate());

  function_template->SetClassName(sandbox_obj->GetConstructorName());

  Local<ObjectTemplate> object_template =
      function_template->InstanceTemplate();

  Local<Object> data_wrapper;
  if (!CreateDataWrapper(env).ToLocal(&data_wrapper))
    return MaybeLocal<Context>();

  NamedPropertyHandlerConfiguration config(PropertyGetterCallback,
                                           PropertySetterCallback,
                                           PropertyDescriptorCallback,
                                           PropertyDeleterCallback,
                                           PropertyEnumeratorCallback,
                                           PropertyDefinerCallback,
                                           data_wrapper);

  IndexedPropertyHandlerConfiguration indexed_config(
      IndexedPropertyGetterCallback,
      IndexedPropertySetterCallback,
      IndexedPropertyDescriptorCallback,
      IndexedPropertyDeleterCallback,
      PropertyEnumeratorCallback,
      IndexedPropertyDefinerCallback,
      data_wrapper);

  object_template->SetHandler(config);
  object_template->SetHandler(indexed_config);

  Local<Context> ctx = NewContext(env->isolate(), object_template);

  if (ctx.IsEmpty()) {
    env->ThrowError("Could not instantiate context");
    return MaybeLocal<Context>();
  }

  ctx->SetSecurityToken(env->context()->GetSecurityToken());

  // We need to tie the lifetime of the sandbox object with the lifetime of
  // newly created context. We do this by making them hold references to each
  // other. The context can directly hold a reference to the sandbox as an
  // embedder data field. However, we cannot hold a reference to a v8::Context
  // directly in an Object, we instead hold onto the new context's global
  // object instead (which then has a reference to the context).
  ctx->SetEmbedderData(ContextEmbedderIndex::kSandboxObject, sandbox_obj);
  sandbox_obj->SetPrivate(env->context(),
                          env->contextify_global_private_symbol(),
                          ctx->Global());

  Utf8Value name_val(env->isolate(), options.name);
  ctx->AllowCodeGenerationFromStrings(options.allow_code_gen_strings->IsTrue());
  ctx->SetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration,
                       options.allow_code_gen_wasm);

  ContextInfo info(*name_val);

  if (!options.origin.IsEmpty()) {
    Utf8Value origin_val(env->isolate(), options.origin);
    info.origin = *origin_val;
  }

  env->AssignToContext(ctx, info);

  return scope.Escape(ctx);
}


void ContextifyContext::Init(Environment* env, Local<Object> target) {
  Local<FunctionTemplate> function_template =
      FunctionTemplate::New(env->isolate());
  function_template->InstanceTemplate()->SetInternalFieldCount(1);
  env->set_script_data_constructor_function(
      function_template->GetFunction(env->context()).ToLocalChecked());

  env->SetMethod(target, "makeContext", MakeContext);
  env->SetMethod(target, "isContext", IsContext);
  env->SetMethod(target, "compileFunction", CompileFunction);
}


// makeContext(sandbox, name, origin, strings, wasm);
void ContextifyContext::MakeContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 5);
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

  TryCatchScope try_catch(env);
  ContextifyContext* context = new ContextifyContext(env, sandbox, options);

  if (try_catch.HasCaught()) {
    try_catch.ReThrow();
    return;
  }

  if (context->context().IsEmpty())
    return;

  sandbox->SetPrivate(
      env->context(),
      env->contextify_context_private_symbol(),
      External::New(env->isolate(), context));
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
  MaybeLocal<Value> maybe_value =
      sandbox->GetPrivate(env->context(),
                          env->contextify_context_private_symbol());
  Local<Value> context_external_v;
  if (maybe_value.ToLocal(&context_external_v) &&
      context_external_v->IsExternal()) {
    Local<External> context_external = context_external_v.As<External>();
    return static_cast<ContextifyContext*>(context_external->Value());
  }
  return nullptr;
}

// static
template <typename T>
ContextifyContext* ContextifyContext::Get(const PropertyCallbackInfo<T>& args) {
  Local<Value> data = args.Data();
  return static_cast<ContextifyContext*>(
      data.As<Object>()->GetAlignedPointerFromInternalField(0));
}

// static
void ContextifyContext::PropertyGetterCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

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
  if (ctx->context_.IsEmpty())
    return;

  auto attributes = PropertyAttribute::None;
  bool is_declared_on_global_proxy = ctx->global_proxy()
      ->GetRealNamedPropertyAttributes(ctx->context(), property)
      .To(&attributes);
  bool read_only =
      static_cast<int>(attributes) &
      static_cast<int>(PropertyAttribute::ReadOnly);

  bool is_declared_on_sandbox = ctx->sandbox()
      ->GetRealNamedPropertyAttributes(ctx->context(), property)
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

  if (!is_declared_on_global_proxy && is_declared_on_sandbox  &&
      args.ShouldThrowOnError() && is_contextual_store && !is_function) {
    // The property exists on the sandbox but not on the global
    // proxy. Setting it would throw because we are in strict mode.
    // Don't attempt to set it by signaling that the call was
    // intercepted. Only change the value on the sandbox.
    args.GetReturnValue().Set(false);
  }

  ctx->sandbox()->Set(ctx->context(), property, value).FromJust();
}

// static
void ContextifyContext::PropertyDescriptorCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  Local<Context> context = ctx->context();

  Local<Object> sandbox = ctx->sandbox();

  if (sandbox->HasOwnProperty(context, property).FromMaybe(false)) {
    args.GetReturnValue().Set(
        sandbox->GetOwnPropertyDescriptor(context, property)
            .ToLocalChecked());
  }
}

// static
void ContextifyContext::PropertyDefinerCallback(
    Local<Name> property,
    const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  Local<Context> context = ctx->context();
  Isolate* isolate = context->GetIsolate();

  auto attributes = PropertyAttribute::None;
  bool is_declared =
      ctx->global_proxy()->GetRealNamedPropertyAttributes(ctx->context(),
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
        sandbox->DefineProperty(context, property, *desc_for_sandbox)
            .FromJust();
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
  if (ctx->context_.IsEmpty())
    return;

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
  if (ctx->context_.IsEmpty())
    return;

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
  if (ctx->context_.IsEmpty())
    return;

  ContextifyContext::PropertyGetterCallback(
      Uint32ToName(ctx->context(), index), args);
}


void ContextifyContext::IndexedPropertySetterCallback(
    uint32_t index,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  ContextifyContext::PropertySetterCallback(
      Uint32ToName(ctx->context(), index), value, args);
}

// static
void ContextifyContext::IndexedPropertyDescriptorCallback(
    uint32_t index,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  ContextifyContext::PropertyDescriptorCallback(
      Uint32ToName(ctx->context(), index), args);
}


void ContextifyContext::IndexedPropertyDefinerCallback(
    uint32_t index,
    const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  ContextifyContext::PropertyDefinerCallback(
      Uint32ToName(ctx->context(), index), desc, args);
}

// static
void ContextifyContext::IndexedPropertyDeleterCallback(
    uint32_t index,
    const PropertyCallbackInfo<Boolean>& args) {
  ContextifyContext* ctx = ContextifyContext::Get(args);

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  Maybe<bool> success = ctx->sandbox()->Delete(ctx->context(), index);

  if (success.FromMaybe(false))
    return;

  // Delete failed on the sandbox, intercept and do not delete on
  // the global object.
  args.GetReturnValue().Set(false);
}

void ContextifyScript::Init(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());
  Local<String> class_name =
      FIXED_ONE_BYTE_STRING(env->isolate(), "ContextifyScript");

  Local<FunctionTemplate> script_tmpl = env->NewFunctionTemplate(New);
  script_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
  script_tmpl->SetClassName(class_name);
  env->SetProtoMethod(script_tmpl, "createCachedData", CreateCachedData);
  env->SetProtoMethod(script_tmpl, "runInContext", RunInContext);
  env->SetProtoMethod(script_tmpl, "runInThisContext", RunInThisContext);

  target->Set(env->context(), class_name,
      script_tmpl->GetFunction(env->context()).ToLocalChecked()).FromJust();
  env->set_script_context_constructor_template(script_tmpl);
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

  Local<Integer> line_offset;
  Local<Integer> column_offset;
  Local<ArrayBufferView> cached_data_buf;
  bool produce_cached_data = false;
  Local<Context> parsing_context = context;

  if (argc > 2) {
    // new ContextifyScript(code, filename, lineOffset, columnOffset,
    //                      cachedData, produceCachedData, parsingContext)
    CHECK_EQ(argc, 7);
    CHECK(args[2]->IsNumber());
    line_offset = args[2].As<Integer>();
    CHECK(args[3]->IsNumber());
    column_offset = args[3].As<Integer>();
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
  } else {
    line_offset = Integer::New(isolate, 0);
    column_offset = Integer::New(isolate, 0);
  }

  ContextifyScript* contextify_script =
      new ContextifyScript(env, args.This());

  if (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACING_CATEGORY_NODE2(vm, script)) != 0) {
    Utf8Value fn(isolate, filename);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        TRACING_CATEGORY_NODE2(vm, script),
        "ContextifyScript::New",
        contextify_script,
        "filename", TRACE_STR_COPY(*fn));
  }

  ScriptCompiler::CachedData* cached_data = nullptr;
  if (!cached_data_buf.IsEmpty()) {
    ArrayBuffer::Contents contents = cached_data_buf->Buffer()->GetContents();
    uint8_t* data = static_cast<uint8_t*>(contents.Data());
    cached_data = new ScriptCompiler::CachedData(
        data + cached_data_buf->ByteOffset(), cached_data_buf->ByteLength());
  }

  Local<PrimitiveArray> host_defined_options =
      PrimitiveArray::New(isolate, loader::HostDefinedOptions::kLength);
  host_defined_options->Set(isolate, loader::HostDefinedOptions::kType,
                            Number::New(isolate, loader::ScriptType::kScript));
  host_defined_options->Set(isolate, loader::HostDefinedOptions::kID,
                            Number::New(isolate, contextify_script->id()));

  ScriptOrigin origin(filename,
                      line_offset,                          // line offset
                      column_offset,                        // column offset
                      True(isolate),                        // is cross origin
                      Local<Integer>(),                     // script id
                      Local<Value>(),                       // source map URL
                      False(isolate),                       // is opaque (?)
                      False(isolate),                       // is WASM
                      False(isolate),                       // is ES Module
                      host_defined_options);
  ScriptCompiler::Source source(code, origin, cached_data);
  ScriptCompiler::CompileOptions compile_options =
      ScriptCompiler::kNoCompileOptions;

  if (source.GetCachedData() != nullptr)
    compile_options = ScriptCompiler::kConsumeCodeCache;

  TryCatchScope try_catch(env);
  Environment::ShouldNotAbortOnUncaughtScope no_abort_scope(env);
  Context::Scope scope(parsing_context);

  MaybeLocal<UnboundScript> v8_script = ScriptCompiler::CompileUnboundScript(
      isolate,
      &source,
      compile_options);

  if (v8_script.IsEmpty()) {
    DecorateErrorStack(env, try_catch);
    no_abort_scope.Close();
    try_catch.ReThrow();
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        TRACING_CATEGORY_NODE2(vm, script),
        "ContextifyScript::New",
        contextify_script);
    return;
  }
  contextify_script->script_.Reset(isolate, v8_script.ToLocalChecked());

  if (compile_options == ScriptCompiler::kConsumeCodeCache) {
    args.This()->Set(
        env->context(),
        env->cached_data_rejected_string(),
        Boolean::New(isolate, source.GetCachedData()->rejected)).FromJust();
  } else if (produce_cached_data) {
    const ScriptCompiler::CachedData* cached_data =
      ScriptCompiler::CreateCodeCache(v8_script.ToLocalChecked());
    bool cached_data_produced = cached_data != nullptr;
    if (cached_data_produced) {
      MaybeLocal<Object> buf = Buffer::Copy(
          env,
          reinterpret_cast<const char*>(cached_data->data),
          cached_data->length);
      args.This()->Set(env->context(),
                       env->cached_data_string(),
                       buf.ToLocalChecked()).FromJust();
    }
    args.This()->Set(
        env->context(),
        env->cached_data_produced_string(),
        Boolean::New(isolate, cached_data_produced)).FromJust();
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      TRACING_CATEGORY_NODE2(vm, script),
      "ContextifyScript::New",
      contextify_script);
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

void ContextifyScript::RunInThisContext(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ContextifyScript* wrapped_script;
  ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACING_CATEGORY_NODE2(vm, script), "RunInThisContext", wrapped_script);

  // TODO(addaleax): Use an options object or otherwise merge this with
  // RunInContext().
  CHECK_EQ(args.Length(), 4);

  CHECK(args[0]->IsNumber());
  int64_t timeout = args[0]->IntegerValue(env->context()).FromJust();

  CHECK(args[1]->IsBoolean());
  bool display_errors = args[1]->IsTrue();

  CHECK(args[2]->IsBoolean());
  bool break_on_sigint = args[2]->IsTrue();

  CHECK(args[3]->IsBoolean());
  bool break_on_first_line = args[3]->IsTrue();

  // Do the eval within this context
  EvalMachine(env,
              timeout,
              display_errors,
              break_on_sigint,
              break_on_first_line,
              args);

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      TRACING_CATEGORY_NODE2(vm, script), "RunInThisContext", wrapped_script);
}

void ContextifyScript::RunInContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ContextifyScript* wrapped_script;
  ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder());

  CHECK_EQ(args.Length(), 5);

  CHECK(args[0]->IsObject());
  Local<Object> sandbox = args[0].As<Object>();
  // Get the context from the sandbox
  ContextifyContext* contextify_context =
      ContextifyContext::ContextFromContextifiedSandbox(env, sandbox);
  CHECK_NOT_NULL(contextify_context);

  if (contextify_context->context().IsEmpty())
    return;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACING_CATEGORY_NODE2(vm, script), "RunInContext", wrapped_script);

  CHECK(args[1]->IsNumber());
  int64_t timeout = args[1]->IntegerValue(env->context()).FromJust();

  CHECK(args[2]->IsBoolean());
  bool display_errors = args[2]->IsTrue();

  CHECK(args[3]->IsBoolean());
  bool break_on_sigint = args[3]->IsTrue();

  CHECK(args[4]->IsBoolean());
  bool break_on_first_line = args[4]->IsTrue();

  // Do the eval within the context
  Context::Scope context_scope(contextify_context->context());
  EvalMachine(contextify_context->env(),
              timeout,
              display_errors,
              break_on_sigint,
              break_on_first_line,
              args);

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      TRACING_CATEGORY_NODE2(vm, script), "RunInContext", wrapped_script);
}

bool ContextifyScript::EvalMachine(Environment* env,
                                   const int64_t timeout,
                                   const bool display_errors,
                                   const bool break_on_sigint,
                                   const bool break_on_first_line,
                                   const FunctionCallbackInfo<Value>& args) {
  if (!env->can_call_into_js())
    return false;
  if (!ContextifyScript::InstanceOf(env, args.Holder())) {
    env->ThrowTypeError(
        "Script methods can only be called on script instances.");
    return false;
  }
  TryCatchScope try_catch(env);
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
  if (break_on_sigint && timeout != -1) {
    Watchdog wd(env->isolate(), timeout, &timed_out);
    SigintWatchdog swd(env->isolate(), &received_signal);
    result = script->Run(env->context());
  } else if (break_on_sigint) {
    SigintWatchdog swd(env->isolate(), &received_signal);
    result = script->Run(env->context());
  } else if (timeout != -1) {
    Watchdog wd(env->isolate(), timeout, &timed_out);
    result = script->Run(env->context());
  } else {
    result = script->Run(env->context());
  }

  // Convert the termination exception into a regular exception.
  if (timed_out || received_signal) {
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
      DecorateErrorStack(env, try_catch);
    }

    // If there was an exception thrown during script execution, re-throw it.
    // If one of the above checks threw, re-throw the exception instead of
    // letting try_catch catch it.
    // If execution has been terminated, but not by one of the watchdogs from
    // this invocation, this will re-throw a `null` value.
    try_catch.ReThrow();

    return false;
  }

  args.GetReturnValue().Set(result.ToLocalChecked());
  return true;
}


ContextifyScript::ContextifyScript(Environment* env, Local<Object> object)
    : BaseObject(env, object),
      id_(env->get_next_script_id()) {
  MakeWeak();
  env->id_to_script_map.emplace(id_, this);
}


ContextifyScript::~ContextifyScript() {
  env()->id_to_script_map.erase(id_);
}


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
  Local<Integer> line_offset = args[2].As<Integer>();

  // Argument 4: column offset
  CHECK(args[3]->IsNumber());
  Local<Integer> column_offset = args[3].As<Integer>();

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

  // Read cache from cached data buffer
  ScriptCompiler::CachedData* cached_data = nullptr;
  if (!cached_data_buf.IsEmpty()) {
    ArrayBuffer::Contents contents = cached_data_buf->Buffer()->GetContents();
    uint8_t* data = static_cast<uint8_t*>(contents.Data());
    cached_data = new ScriptCompiler::CachedData(
      data + cached_data_buf->ByteOffset(), cached_data_buf->ByteLength());
  }

  ScriptOrigin origin(filename, line_offset, column_offset, True(isolate));
  ScriptCompiler::Source source(code, origin, cached_data);
  ScriptCompiler::CompileOptions options;
  if (source.GetCachedData() == nullptr) {
    options = ScriptCompiler::kNoCompileOptions;
  } else {
    options = ScriptCompiler::kConsumeCodeCache;
  }

  TryCatchScope try_catch(env);
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

  MaybeLocal<Function> maybe_fun = ScriptCompiler::CompileFunctionInContext(
      parsing_context, &source, params.size(), params.data(),
      context_extensions.size(), context_extensions.data(), options);

  Local<Function> fun;
  if (maybe_fun.IsEmpty() || !maybe_fun.ToLocal(&fun)) {
    DecorateErrorStack(env, try_catch);
    try_catch.ReThrow();
    return;
  }

  if (produce_cached_data) {
    const std::unique_ptr<ScriptCompiler::CachedData> cached_data(
        ScriptCompiler::CreateCodeCacheForFunction(fun));
    bool cached_data_produced = cached_data != nullptr;
    if (cached_data_produced) {
      MaybeLocal<Object> buf = Buffer::Copy(
          env,
          reinterpret_cast<const char*>(cached_data->data),
          cached_data->length);
      if (fun->Set(
          parsing_context,
          env->cached_data_string(),
          buf.ToLocalChecked()).IsNothing()) return;
    }
    if (fun->Set(
        parsing_context,
        env->cached_data_produced_string(),
        Boolean::New(isolate, cached_data_produced)).IsNothing()) return;
  }

  args.GetReturnValue().Set(fun);
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  ContextifyContext::Init(env, target);
  ContextifyScript::Init(env, target);
}

}  // namespace contextify
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(contextify, node::contextify::Initialize)

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

#include "node_internals.h"
#include "node_watchdog.h"
#include "base_object-inl.h"
#include "node_contextify.h"
#include "node_context_data.h"
#include "node_errors.h"

namespace node {
namespace contextify {

using errors::NodeTryCatch;

using v8::Array;
using v8::ArrayBuffer;
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
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::PropertyDescriptor;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Symbol;
using v8::Uint32;
using v8::Uint8Array;
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

ContextifyContext::ContextifyContext(
    Environment* env,
    Local<Object> sandbox_obj, const ContextOptions& options) : env_(env) {
  Local<Context> v8_context = CreateV8Context(env, sandbox_obj, options);
  context_.Reset(env->isolate(), v8_context);

  // Allocation failure or maximum call stack size reached
  if (context_.IsEmpty())
    return;
  context_.SetWeak(this, WeakCallback, v8::WeakCallbackType::kParameter);
  context_.MarkIndependent();
}


// This is an object that just keeps an internal pointer to this
// ContextifyContext.  It's passed to the NamedPropertyHandler.  If we
// pass the main JavaScript context object we're embedded in, then the
// NamedPropertyHandler will store a reference to it forever and keep it
// from getting gc'd.
Local<Value> ContextifyContext::CreateDataWrapper(Environment* env) {
  EscapableHandleScope scope(env->isolate());
  Local<Object> wrapper =
      env->script_data_constructor_function()
          ->NewInstance(env->context()).FromMaybe(Local<Object>());
  if (wrapper.IsEmpty())
    return scope.Escape(Local<Value>::New(env->isolate(), Local<Value>()));

  Wrap(wrapper, this);
  return scope.Escape(wrapper);
}


Local<Context> ContextifyContext::CreateV8Context(
    Environment* env,
    Local<Object> sandbox_obj,
    const ContextOptions& options) {
  EscapableHandleScope scope(env->isolate());
  Local<FunctionTemplate> function_template =
      FunctionTemplate::New(env->isolate());

  function_template->SetClassName(sandbox_obj->GetConstructorName());

  Local<ObjectTemplate> object_template =
      function_template->InstanceTemplate();

  NamedPropertyHandlerConfiguration config(PropertyGetterCallback,
                                           PropertySetterCallback,
                                           PropertyDescriptorCallback,
                                           PropertyDeleterCallback,
                                           PropertyEnumeratorCallback,
                                           PropertyDefinerCallback,
                                           CreateDataWrapper(env));

  IndexedPropertyHandlerConfiguration indexed_config(
      IndexedPropertyGetterCallback,
      IndexedPropertySetterCallback,
      IndexedPropertyDescriptorCallback,
      IndexedPropertyDeleterCallback,
      PropertyEnumeratorCallback,
      IndexedPropertyDefinerCallback,
      CreateDataWrapper(env));

  object_template->SetHandler(config);
  object_template->SetHandler(indexed_config);

  Local<Context> ctx = NewContext(env->isolate(), object_template);

  if (ctx.IsEmpty()) {
    env->ThrowError("Could not instantiate context");
    return Local<Context>();
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
  env->set_script_data_constructor_function(function_template->GetFunction());

  env->SetMethod(target, "makeContext", MakeContext);
  env->SetMethod(target, "isContext", IsContext);
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

  NodeTryCatch try_catch(env);
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
void ContextifyContext::PropertyGetterCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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

  ctx->sandbox()->Set(property, value);
}

// static
void ContextifyContext::PropertyDescriptorCallback(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  Local<Context> context = ctx->context();
  v8::Isolate* isolate = context->GetIsolate();

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
        desc.has_get() ? desc.get() : v8::Undefined(isolate).As<Value>(),
        desc.has_set() ? desc.set() : v8::Undefined(isolate).As<Value>());

    define_prop_on_sandbox(&desc_for_sandbox);
  } else {
    Local<Value> value =
        desc.has_value() ? desc.value() : v8::Undefined(isolate).As<Value>();

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

  // Still initializing
  if (ctx->context_.IsEmpty())
    return;

  args.GetReturnValue().Set(ctx->sandbox()->GetPropertyNames());
}

// static
void ContextifyContext::IndexedPropertyGetterCallback(
    uint32_t index,
    const PropertyCallbackInfo<Value>& args) {
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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
  ContextifyContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

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

Maybe<bool> GetBreakOnSigintArg(Environment* env,
                                Local<Value> options) {
  if (options->IsUndefined() || options->IsString()) {
    return Just(false);
  }
  if (!options->IsObject()) {
    env->ThrowTypeError("options must be an object");
    return Nothing<bool>();
  }

  Local<String> key = FIXED_ONE_BYTE_STRING(env->isolate(), "breakOnSigint");
  MaybeLocal<Value> maybe_value =
      options.As<Object>()->Get(env->context(), key);
  if (maybe_value.IsEmpty())
    return Nothing<bool>();

  Local<Value> value = maybe_value.ToLocalChecked();
  return Just(value->IsTrue());
}

Maybe<int64_t> GetTimeoutArg(Environment* env, Local<Value> options) {
  if (options->IsUndefined() || options->IsString()) {
    return Just<int64_t>(-1);
  }
  if (!options->IsObject()) {
    env->ThrowTypeError("options must be an object");
    return Nothing<int64_t>();
  }

  MaybeLocal<Value> maybe_value =
      options.As<Object>()->Get(env->context(), env->timeout_string());
  if (maybe_value.IsEmpty())
    return Nothing<int64_t>();

  Local<Value> value = maybe_value.ToLocalChecked();
  if (value->IsUndefined()) {
    return Just<int64_t>(-1);
  }

  Maybe<int64_t> timeout = value->IntegerValue(env->context());

  if (timeout.IsJust() && timeout.ToChecked() <= 0) {
    env->ThrowRangeError("timeout must be a positive number");
    return Nothing<int64_t>();
  }

  return timeout;
}

MaybeLocal<Integer> GetLineOffsetArg(Environment* env,
                                     Local<Value> options) {
  Local<Integer> defaultLineOffset = Integer::New(env->isolate(), 0);

  if (!options->IsObject()) {
    return defaultLineOffset;
  }

  Local<String> key = FIXED_ONE_BYTE_STRING(env->isolate(), "lineOffset");
  MaybeLocal<Value> maybe_value =
      options.As<Object>()->Get(env->context(), key);
  if (maybe_value.IsEmpty())
    return MaybeLocal<Integer>();

  Local<Value> value = maybe_value.ToLocalChecked();
  if (value->IsUndefined())
    return defaultLineOffset;

  return value->ToInteger(env->context());
}

MaybeLocal<Integer> GetColumnOffsetArg(Environment* env,
                                       Local<Value> options) {
  Local<Integer> defaultColumnOffset = Integer::New(env->isolate(), 0);

  if (!options->IsObject()) {
    return defaultColumnOffset;
  }

  Local<String> key = FIXED_ONE_BYTE_STRING(env->isolate(), "columnOffset");
  MaybeLocal<Value> maybe_value =
    options.As<Object>()->Get(env->context(), key);
  if (maybe_value.IsEmpty())
    return MaybeLocal<Integer>();

  Local<Value> value = maybe_value.ToLocalChecked();
  if (value->IsUndefined())
    return defaultColumnOffset;

  return value->ToInteger(env->context());
}

MaybeLocal<Context> GetContextArg(Environment* env,
                                  Local<Value> options) {
  if (!options->IsObject())
    return MaybeLocal<Context>();

  MaybeLocal<Value> maybe_value =
      options.As<Object>()->Get(env->context(),
                                env->vm_parsing_context_symbol());
  Local<Value> value;
  if (!maybe_value.ToLocal(&value))
    return MaybeLocal<Context>();

  if (!value->IsObject()) {
    if (!value->IsNullOrUndefined()) {
      env->ThrowTypeError(
          "contextifiedSandbox argument must be an object.");
    }
    return MaybeLocal<Context>();
  }

  ContextifyContext* sandbox =
      ContextifyContext::ContextFromContextifiedSandbox(
          env, value.As<Object>());
  if (!sandbox) {
    env->ThrowTypeError(
        "sandbox argument must have been converted to a context.");
    return MaybeLocal<Context>();
  }

  Local<Context> context = sandbox->context();
  if (context.IsEmpty())
    return MaybeLocal<Context>();
  return context;
}

class ContextifyScript : public BaseObject {
 private:
  Persistent<UnboundScript> script_;

 public:
  static void Init(Environment* env, Local<Object> target) {
    HandleScope scope(env->isolate());
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(env->isolate(), "ContextifyScript");

    Local<FunctionTemplate> script_tmpl = env->NewFunctionTemplate(New);
    script_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    script_tmpl->SetClassName(class_name);
    env->SetProtoMethod(script_tmpl, "runInContext", RunInContext);
    env->SetProtoMethod(script_tmpl, "runInThisContext", RunInThisContext);

    target->Set(class_name, script_tmpl->GetFunction());
    env->set_script_context_constructor_template(script_tmpl);

    Local<Symbol> parsing_context_symbol =
        Symbol::New(env->isolate(),
                    FIXED_ONE_BYTE_STRING(env->isolate(),
                                          "script parsing context"));
    env->set_vm_parsing_context_symbol(parsing_context_symbol);
    target->Set(env->context(),
                FIXED_ONE_BYTE_STRING(env->isolate(), "kParsingContext"),
                parsing_context_symbol)
        .FromJust();
  }


  static void New(const FunctionCallbackInfo<Value>& args) {
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
    Local<Uint8Array> cached_data_buf;
    bool produce_cached_data = false;
    Local<Context> parsing_context = context;

    if (argc > 2) {
      // new ContextifyScript(code, filename, lineOffset, columnOffset
      //                      cachedData, produceCachedData, parsingContext)
      CHECK_EQ(argc, 7);
      CHECK(args[2]->IsNumber());
      line_offset = args[2].As<Integer>();
      CHECK(args[3]->IsNumber());
      column_offset = args[3].As<Integer>();
      if (!args[4]->IsUndefined()) {
        CHECK(args[4]->IsUint8Array());
        cached_data_buf = args[4].As<Uint8Array>();
      }
      CHECK(args[5]->IsBoolean());
      produce_cached_data = args[5]->IsTrue();
      if (!args[6]->IsUndefined()) {
        CHECK(args[6]->IsObject());
        ContextifyContext* sandbox =
            ContextifyContext::ContextFromContextifiedSandbox(
                env, args[6].As<Object>());
        CHECK_NE(sandbox, nullptr);
        parsing_context = sandbox->context();
      }
    } else {
      line_offset = Integer::New(isolate, 0);
      column_offset = Integer::New(isolate, 0);
    }

    ContextifyScript* contextify_script =
        new ContextifyScript(env, args.This());

    ScriptCompiler::CachedData* cached_data = nullptr;
    if (!cached_data_buf.IsEmpty()) {
      ArrayBuffer::Contents contents = cached_data_buf->Buffer()->GetContents();
      uint8_t* data = static_cast<uint8_t*>(contents.Data());
      cached_data = new ScriptCompiler::CachedData(
          data + cached_data_buf->ByteOffset(), cached_data_buf->ByteLength());
    }

    ScriptOrigin origin(filename, line_offset, column_offset);
    ScriptCompiler::Source source(code, origin, cached_data);
    ScriptCompiler::CompileOptions compile_options =
        ScriptCompiler::kNoCompileOptions;

    if (source.GetCachedData() != nullptr)
      compile_options = ScriptCompiler::kConsumeCodeCache;

    NodeTryCatch try_catch(env);
    Environment::ShouldNotAbortOnUncaughtScope no_abort_scope(env);
    Context::Scope scope(parsing_context);

    MaybeLocal<UnboundScript> v8_script = ScriptCompiler::CompileUnboundScript(
        isolate,
        &source,
        compile_options);

    if (v8_script.IsEmpty()) {
      no_abort_scope.Close();
      try_catch.ReThrow();
      return;
    }
    contextify_script->script_.Reset(isolate, v8_script.ToLocalChecked());

    if (compile_options == ScriptCompiler::kConsumeCodeCache) {
      args.This()->Set(
          env->cached_data_rejected_string(),
          Boolean::New(isolate, source.GetCachedData()->rejected));
    } else if (produce_cached_data) {
      const ScriptCompiler::CachedData* cached_data =
        ScriptCompiler::CreateCodeCache(v8_script.ToLocalChecked(), code);
      bool cached_data_produced = cached_data != nullptr;
      if (cached_data_produced) {
        MaybeLocal<Object> buf = Buffer::Copy(
            env,
            reinterpret_cast<const char*>(cached_data->data),
            cached_data->length);
        args.This()->Set(env->cached_data_string(), buf.ToLocalChecked());
      }
      args.This()->Set(
          env->cached_data_produced_string(),
          Boolean::New(isolate, cached_data_produced));
    }
  }


  static bool InstanceOf(Environment* env, const Local<Value>& value) {
    return !value.IsEmpty() &&
           env->script_context_constructor_template()->HasInstance(value);
  }


  static void RunInThisContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    CHECK_EQ(args.Length(), 3);

    CHECK(args[0]->IsNumber());
    int64_t timeout = args[0]->IntegerValue(env->context()).FromJust();

    CHECK(args[1]->IsBoolean());
    bool display_errors = args[1]->IsTrue();

    CHECK(args[2]->IsBoolean());
    bool break_on_sigint = args[2]->IsTrue();

    // Do the eval within this context
    EvalMachine(env, timeout, display_errors, break_on_sigint, args);
  }

  static void RunInContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    CHECK_EQ(args.Length(), 4);

    CHECK(args[0]->IsObject());
    Local<Object> sandbox = args[0].As<Object>();
    // Get the context from the sandbox
    ContextifyContext* contextify_context =
        ContextifyContext::ContextFromContextifiedSandbox(env, sandbox);
    CHECK_NE(contextify_context, nullptr);

    if (contextify_context->context().IsEmpty())
      return;

    CHECK(args[1]->IsNumber());
    int64_t timeout = args[1]->IntegerValue(env->context()).FromJust();

    CHECK(args[2]->IsBoolean());
    bool display_errors = args[2]->IsTrue();

    CHECK(args[3]->IsBoolean());
    bool break_on_sigint = args[3]->IsTrue();

    // Do the eval within the context
    Context::Scope context_scope(contextify_context->context());
    EvalMachine(contextify_context->env(),
                timeout,
                display_errors,
                break_on_sigint,
                args);
  }


  static bool EvalMachine(Environment* env,
                          const int64_t timeout,
                          const bool display_errors,
                          const bool break_on_sigint,
                          const FunctionCallbackInfo<Value>& args) {
    if (!ContextifyScript::InstanceOf(env, args.Holder())) {
      env->ThrowTypeError(
          "Script methods can only be called on script instances.");
      return false;
    }
    NodeTryCatch try_catch(env);
    ContextifyScript* wrapped_script;
    ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder(), false);
    Local<UnboundScript> unbound_script =
        PersistentToLocal(env->isolate(), wrapped_script->script_);
    Local<Script> script = unbound_script->BindToCurrentContext();

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

    if (timed_out || received_signal) {
      // It is possible that execution was terminated by another timeout in
      // which this timeout is nested, so check whether one of the watchdogs
      // from this invocation is responsible for termination.
      if (timed_out) {
        env->ThrowError("Script execution timed out.");
      } else if (received_signal) {
        env->ThrowError("Script execution interrupted.");
      }
      env->isolate()->CancelTerminateExecution();
    }

    if (try_catch.HasCaught()) {
      // If there was an exception thrown during script execution, re-throw it.
      // If one of the above checks threw, re-throw the exception instead of
      // letting try_catch catch it.
      // If execution has been terminated, but not by one of the watchdogs from
      // this invocation, this will re-throw a `null` value.
      try_catch.ReThrow(true);

      return false;
    }

    args.GetReturnValue().Set(result.ToLocalChecked());
    return true;
  }


  ContextifyScript(Environment* env, Local<Object> object)
      : BaseObject(env, object) {
    MakeWeak<ContextifyScript>(this);
  }
};


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  ContextifyContext::Init(env, target);
  ContextifyScript::Init(env, target);
}

}  // namespace contextify
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(contextify, node::contextify::Initialize)

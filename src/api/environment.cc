#include "env.h"
#include "node.h"
#include "node_context_data.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_native_module.h"
#include "node_platform.h"
#include "node_process.h"
#include "node_v8_platform-inl.h"
#include "uv.h"

#ifdef NODE_ENABLE_VTUNE_PROFILING
#include "../deps/v8/src/third_party/vtune/v8-vtune.h"
#endif

namespace node {
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Message;
using v8::MicrotasksPolicy;
using v8::Object;
using v8::ObjectTemplate;
using v8::Private;
using v8::String;
using v8::Value;

static bool AllowWasmCodeGenerationCallback(Local<Context> context,
                                            Local<String>) {
  Local<Value> wasm_code_gen =
      context->GetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration);
  return wasm_code_gen->IsUndefined() || wasm_code_gen->IsTrue();
}

static bool ShouldAbortOnUncaughtException(Isolate* isolate) {
  DebugSealHandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  return env != nullptr &&
         (env->is_main_thread() || !env->is_stopping()) &&
         env->should_abort_on_uncaught_toggle()[0] &&
         !env->inside_should_not_abort_on_uncaught_scope();
}

static void OnMessage(Local<Message> message, Local<Value> error) {
  Isolate* isolate = message->GetIsolate();
  switch (message->ErrorLevel()) {
    case Isolate::MessageErrorLevel::kMessageWarning: {
      Environment* env = Environment::GetCurrent(isolate);
      if (!env) {
        break;
      }
      Utf8Value filename(isolate, message->GetScriptOrigin().ResourceName());
      // (filename):(line) (message)
      std::stringstream warning;
      warning << *filename;
      warning << ":";
      warning << message->GetLineNumber(env->context()).FromMaybe(-1);
      warning << " ";
      v8::String::Utf8Value msg(isolate, message->Get());
      warning << *msg;
      USE(ProcessEmitWarningGeneric(env, warning.str().c_str(), "V8"));
      break;
    }
    case Isolate::MessageErrorLevel::kMessageError:
      FatalException(isolate, error, message);
      break;
  }
}

void* NodeArrayBufferAllocator::Allocate(size_t size) {
  if (zero_fill_field_ || per_process::cli_options->zero_fill_all_buffers)
    return UncheckedCalloc(size);
  else
    return UncheckedMalloc(size);
}

DebuggingArrayBufferAllocator::~DebuggingArrayBufferAllocator() {
  CHECK(allocations_.empty());
}

void* DebuggingArrayBufferAllocator::Allocate(size_t size) {
  Mutex::ScopedLock lock(mutex_);
  void* data = NodeArrayBufferAllocator::Allocate(size);
  RegisterPointerInternal(data, size);
  return data;
}

void* DebuggingArrayBufferAllocator::AllocateUninitialized(size_t size) {
  Mutex::ScopedLock lock(mutex_);
  void* data = NodeArrayBufferAllocator::AllocateUninitialized(size);
  RegisterPointerInternal(data, size);
  return data;
}

void DebuggingArrayBufferAllocator::Free(void* data, size_t size) {
  Mutex::ScopedLock lock(mutex_);
  UnregisterPointerInternal(data, size);
  NodeArrayBufferAllocator::Free(data, size);
}

void* DebuggingArrayBufferAllocator::Reallocate(void* data,
                                                size_t old_size,
                                                size_t size) {
  Mutex::ScopedLock lock(mutex_);
  void* ret = NodeArrayBufferAllocator::Reallocate(data, old_size, size);
  if (ret == nullptr) {
    if (size == 0)  // i.e. equivalent to free().
      UnregisterPointerInternal(data, old_size);
    return nullptr;
  }

  if (data != nullptr) {
    auto it = allocations_.find(data);
    CHECK_NE(it, allocations_.end());
    allocations_.erase(it);
  }

  RegisterPointerInternal(ret, size);
  return ret;
}

void DebuggingArrayBufferAllocator::RegisterPointer(void* data, size_t size) {
  Mutex::ScopedLock lock(mutex_);
  RegisterPointerInternal(data, size);
}

void DebuggingArrayBufferAllocator::UnregisterPointer(void* data, size_t size) {
  Mutex::ScopedLock lock(mutex_);
  UnregisterPointerInternal(data, size);
}

void DebuggingArrayBufferAllocator::UnregisterPointerInternal(void* data,
                                                              size_t size) {
  if (data == nullptr) return;
  auto it = allocations_.find(data);
  CHECK_NE(it, allocations_.end());
  CHECK_EQ(it->second, size);
  allocations_.erase(it);
}

void DebuggingArrayBufferAllocator::RegisterPointerInternal(void* data,
                                                            size_t size) {
  if (data == nullptr) return;
  CHECK_EQ(allocations_.count(data), 0);
  allocations_[data] = size;
}

std::unique_ptr<ArrayBufferAllocator> ArrayBufferAllocator::Create(bool debug) {
  if (debug || per_process::cli_options->debug_arraybuffer_allocations)
    return std::make_unique<DebuggingArrayBufferAllocator>();
  else
    return std::make_unique<NodeArrayBufferAllocator>();
}

ArrayBufferAllocator* CreateArrayBufferAllocator() {
  return ArrayBufferAllocator::Create().release();
}

void FreeArrayBufferAllocator(ArrayBufferAllocator* allocator) {
  delete allocator;
}

void SetIsolateCreateParams(Isolate::CreateParams* params,
                            ArrayBufferAllocator* allocator) {
  if (allocator != nullptr)
    params->array_buffer_allocator = allocator;

#ifdef NODE_ENABLE_VTUNE_PROFILING
  params->code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif
}

void SetIsolateUpForNode(v8::Isolate* isolate) {
  isolate->AddMessageListenerWithErrorLevel(
      OnMessage,
      Isolate::MessageErrorLevel::kMessageError |
          Isolate::MessageErrorLevel::kMessageWarning);
  isolate->SetAbortOnUncaughtExceptionCallback(ShouldAbortOnUncaughtException);
  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);
  isolate->SetFatalErrorHandler(OnFatalError);
  isolate->SetAllowWasmCodeGenerationCallback(AllowWasmCodeGenerationCallback);
  isolate->SetPromiseRejectCallback(task_queue::PromiseRejectCallback);
  v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
}

Isolate* NewIsolate(ArrayBufferAllocator* allocator, uv_loop_t* event_loop) {
  return NewIsolate(allocator, event_loop, GetMainThreadMultiIsolatePlatform());
}

Isolate* NewIsolate(ArrayBufferAllocator* allocator,
                    uv_loop_t* event_loop,
                    MultiIsolatePlatform* platform) {
  Isolate::CreateParams params;
  SetIsolateCreateParams(&params, allocator);

  Isolate* isolate = Isolate::Allocate();
  if (isolate == nullptr) return nullptr;

  // Register the isolate on the platform before the isolate gets initialized,
  // so that the isolate can access the platform during initialization.
  platform->RegisterIsolate(isolate, event_loop);
  Isolate::Initialize(isolate, params);

  SetIsolateUpForNode(isolate);

  return isolate;
}

IsolateData* CreateIsolateData(Isolate* isolate,
                               uv_loop_t* loop,
                               MultiIsolatePlatform* platform,
                               ArrayBufferAllocator* allocator) {
  return new IsolateData(isolate, loop, platform, allocator);
}

void FreeIsolateData(IsolateData* isolate_data) {
  delete isolate_data;
}

Environment* CreateEnvironment(IsolateData* isolate_data,
                               Local<Context> context,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  Isolate* isolate = context->GetIsolate();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);
  // TODO(addaleax): This is a much better place for parsing per-Environment
  // options than the global parse call.
  std::vector<std::string> args(argv, argv + argc);
  std::vector<std::string> exec_args(exec_argv, exec_argv + exec_argc);
  // TODO(addaleax): Provide more sensible flags, in an embedder-accessible way.
  Environment* env = new Environment(
      isolate_data,
      context,
      static_cast<Environment::Flags>(Environment::kIsMainThread |
                                      Environment::kOwnsProcessState |
                                      Environment::kOwnsInspector));
  env->InitializeLibuv(per_process::v8_is_profiling);
  env->ProcessCliArgs(args, exec_args);
  if (RunBootstrapping(env).IsEmpty()) {
    return nullptr;
  }

  std::vector<Local<String>> parameters = {
      env->require_string(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "markBootstrapComplete")};
  std::vector<Local<Value>> arguments = {
      env->native_module_require(),
      env->NewFunctionTemplate(MarkBootstrapComplete)
          ->GetFunction(env->context())
          .ToLocalChecked()};
  if (ExecuteBootstrapper(
          env, "internal/bootstrap/environment", &parameters, &arguments)
          .IsEmpty()) {
    return nullptr;
  }
  return env;
}

void FreeEnvironment(Environment* env) {
  env->RunCleanup();
  delete env;
}

Environment* GetCurrentEnvironment(Local<Context> context) {
  return Environment::GetCurrent(context);
}

MultiIsolatePlatform* GetMainThreadMultiIsolatePlatform() {
  return per_process::v8_platform.Platform();
}

MultiIsolatePlatform* CreatePlatform(
    int thread_pool_size,
    node::tracing::TracingController* tracing_controller) {
  return new NodePlatform(thread_pool_size, tracing_controller);
}

MultiIsolatePlatform* InitializeV8Platform(int thread_pool_size) {
  per_process::v8_platform.Initialize(thread_pool_size);
  return per_process::v8_platform.Platform();
}

void FreePlatform(MultiIsolatePlatform* platform) {
  delete platform;
}

MaybeLocal<Object> GetPerContextExports(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope handle_scope(isolate);

  Local<Object> global = context->Global();
  Local<Private> key = Private::ForApi(isolate,
      FIXED_ONE_BYTE_STRING(isolate, "node:per_context_binding_exports"));

  Local<Value> existing_value;
  if (!global->GetPrivate(context, key).ToLocal(&existing_value))
    return MaybeLocal<Object>();
  if (existing_value->IsObject())
    return handle_scope.Escape(existing_value.As<Object>());

  Local<Object> exports = Object::New(isolate);
  if (context->Global()->SetPrivate(context, key, exports).IsNothing())
    return MaybeLocal<Object>();
  return handle_scope.Escape(exports);
}

Local<Context> NewContext(Isolate* isolate,
                          Local<ObjectTemplate> object_template) {
  auto context = Context::New(isolate, nullptr, object_template);
  if (context.IsEmpty()) return context;
  HandleScope handle_scope(isolate);

  context->SetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration,
                           True(isolate));

  {
    // Run per-context JS files.
    Context::Scope context_scope(context);
    Local<Object> exports;
    if (!GetPerContextExports(context).ToLocal(&exports))
      return Local<Context>();

    Local<String> global_string = FIXED_ONE_BYTE_STRING(isolate, "global");
    Local<String> exports_string = FIXED_ONE_BYTE_STRING(isolate, "exports");

    static const char* context_files[] = {
      "internal/per_context/setup",
      "internal/per_context/domexception",
      nullptr
    };

    for (const char** module = context_files; *module != nullptr; module++) {
      std::vector<Local<String>> parameters = {
        global_string,
        exports_string
      };
      Local<Value> arguments[] = {context->Global(), exports};
      MaybeLocal<Function> maybe_fn =
          per_process::native_module_loader.LookupAndCompile(
              context, *module, &parameters, nullptr);
      if (maybe_fn.IsEmpty()) {
        return Local<Context>();
      }
      Local<Function> fn = maybe_fn.ToLocalChecked();
      MaybeLocal<Value> result =
          fn->Call(context, Undefined(isolate),
                   arraysize(arguments), arguments);
      // Execution failed during context creation.
      // TODO(joyeecheung): deprecate this signature and return a MaybeLocal.
      if (result.IsEmpty()) {
        return Local<Context>();
      }
    }
  }

  return context;
}

uv_loop_t* GetCurrentEventLoop(Isolate* isolate) {
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) return nullptr;
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr) return nullptr;
  return env->event_loop();
}

}  // namespace node

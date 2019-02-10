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

namespace node {
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Message;
using v8::MicrotasksPolicy;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

static bool AllowWasmCodeGenerationCallback(Local<Context> context,
                                            Local<String>) {
  Local<Value> wasm_code_gen =
      context->GetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration);
  return wasm_code_gen->IsUndefined() || wasm_code_gen->IsTrue();
}

static bool ShouldAbortOnUncaughtException(Isolate* isolate) {
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  return env != nullptr && env->should_abort_on_uncaught_toggle()[0] &&
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

void* ArrayBufferAllocator::Allocate(size_t size) {
  if (zero_fill_field_ || per_process::cli_options->zero_fill_all_buffers)
    return UncheckedCalloc(size);
  else
    return UncheckedMalloc(size);
}

ArrayBufferAllocator* CreateArrayBufferAllocator() {
  return new ArrayBufferAllocator();
}

void FreeArrayBufferAllocator(ArrayBufferAllocator* allocator) {
  delete allocator;
}

Isolate* NewIsolate(ArrayBufferAllocator* allocator, uv_loop_t* event_loop) {
  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;

  double total_memory = uv_get_total_memory();
  if (total_memory > 0) {
    // V8 defaults to 700MB or 1.4GB on 32 and 64 bit platforms respectively.
    // This default is based on browser use-cases. Tell V8 to configure the
    // heap based on the actual physical memory.
    params.constraints.ConfigureDefaults(total_memory, 0);
  }

#ifdef NODE_ENABLE_VTUNE_PROFILING
  params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif

  Isolate* isolate = Isolate::Allocate();
  if (isolate == nullptr) return nullptr;

  // Register the isolate on the platform before the isolate gets initialized,
  // so that the isolate can access the platform during initialization.
  per_process::v8_platform.Platform()->RegisterIsolate(isolate, event_loop);
  Isolate::Initialize(isolate, params);

  isolate->AddMessageListenerWithErrorLevel(
      OnMessage,
      Isolate::MessageErrorLevel::kMessageError |
          Isolate::MessageErrorLevel::kMessageWarning);
  isolate->SetAbortOnUncaughtExceptionCallback(ShouldAbortOnUncaughtException);
  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);
  isolate->SetFatalErrorHandler(OnFatalError);
  isolate->SetAllowWasmCodeGenerationCallback(AllowWasmCodeGenerationCallback);
  v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);

  return isolate;
}

IsolateData* CreateIsolateData(Isolate* isolate,
                               uv_loop_t* loop,
                               MultiIsolatePlatform* platform,
                               ArrayBufferAllocator* allocator) {
  return new IsolateData(
      isolate,
      loop,
      platform,
      allocator != nullptr ? allocator->zero_fill_field() : nullptr);
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
  env->Start(per_process::v8_is_profiling);
  env->ProcessCliArgs(args, exec_args);
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

Local<Context> NewContext(Isolate* isolate,
                          Local<ObjectTemplate> object_template) {
  auto context = Context::New(isolate, nullptr, object_template);
  if (context.IsEmpty()) return context;
  HandleScope handle_scope(isolate);

  context->SetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration,
                           True(isolate));

  {
    // Run lib/internal/bootstrap/context.js
    Context::Scope context_scope(context);

    std::vector<Local<String>> parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "global")};
    Local<Value> arguments[] = {context->Global()};
    MaybeLocal<Function> maybe_fn =
        per_process::native_module_loader.LookupAndCompile(
            context, "internal/bootstrap/context", &parameters, nullptr);
    if (maybe_fn.IsEmpty()) {
      return Local<Context>();
    }
    Local<Function> fn = maybe_fn.ToLocalChecked();
    MaybeLocal<Value> result =
        fn->Call(context, Undefined(isolate), arraysize(arguments), arguments);
    // Execution failed during context creation.
    // TODO(joyeecheung): deprecate this signature and return a MaybeLocal.
    if (result.IsEmpty()) {
      return Local<Context>();
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

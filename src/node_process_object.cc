#include "env-inl.h"
#include "node_internals.h"
#include "node_options-inl.h"
#include "node_metadata.h"
#include "node_process.h"
#include "node_revert.h"
#include "util-inl.h"

#include <climits>  // PATH_MAX

namespace node {
using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::None;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::SideEffectType;
using v8::String;
using v8::Value;

static void ProcessTitleGetter(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  std::string title = GetProcessTitle("node");
  info.GetReturnValue().Set(
      String::NewFromUtf8(info.GetIsolate(), title.data(),
                          NewStringType::kNormal, title.size())
      .ToLocalChecked());
}

static void ProcessTitleSetter(Local<Name> property,
                               Local<Value> value,
                               const PropertyCallbackInfo<void>& info) {
  node::Utf8Value title(info.GetIsolate(), value);
  TRACE_EVENT_METADATA1(
      "__metadata", "process_name", "name", TRACE_STR_COPY(*title));
  uv_set_process_title(*title);
}

static void DebugPortGetter(Local<Name> property,
                            const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  ExclusiveAccess<HostPort>::Scoped host_port(env->inspector_host_port());
  int port = host_port->port();
  info.GetReturnValue().Set(port);
}

static void DebugPortSetter(Local<Name> property,
                            Local<Value> value,
                            const PropertyCallbackInfo<void>& info) {
  Environment* env = Environment::GetCurrent(info);
  int32_t port = value->Int32Value(env->context()).FromMaybe(0);
  ExclusiveAccess<HostPort>::Scoped host_port(env->inspector_host_port());
  host_port->set_port(static_cast<int>(port));
}

static void GetParentProcessId(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(uv_os_getppid());
}

MaybeLocal<Object> CreateProcessObject(Environment* env) {
  Isolate* isolate = env->isolate();
  EscapableHandleScope scope(isolate);
  Local<Context> context = env->context();

  Local<FunctionTemplate> process_template = FunctionTemplate::New(isolate);
  process_template->SetClassName(env->process_string());
  Local<Function> process_ctor;
  Local<Object> process;
  if (!process_template->GetFunction(context).ToLocal(&process_ctor) ||
      !process_ctor->NewInstance(context).ToLocal(&process)) {
    return MaybeLocal<Object>();
  }

  // process.version
  READONLY_PROPERTY(process,
                    "version",
                    FIXED_ONE_BYTE_STRING(env->isolate(), NODE_VERSION));

  // process.versions
  Local<Object> versions = Object::New(env->isolate());
  READONLY_PROPERTY(process, "versions", versions);

#define V(key)                                                                 \
  if (!per_process::metadata.versions.key.empty()) {                           \
    READONLY_STRING_PROPERTY(                                                  \
        versions, #key, per_process::metadata.versions.key);                   \
  }
  NODE_VERSIONS_KEYS(V)
#undef V

  // process.arch
  READONLY_STRING_PROPERTY(process, "arch", per_process::metadata.arch);

  // process.platform
  READONLY_STRING_PROPERTY(process, "platform", per_process::metadata.platform);

  // process.release
  Local<Object> release = Object::New(env->isolate());
  READONLY_PROPERTY(process, "release", release);
  READONLY_STRING_PROPERTY(release, "name", per_process::metadata.release.name);
#if NODE_VERSION_IS_LTS
  READONLY_STRING_PROPERTY(release, "lts", per_process::metadata.release.lts);
#endif  // NODE_VERSION_IS_LTS

#ifdef NODE_HAS_RELEASE_URLS
  READONLY_STRING_PROPERTY(
      release, "sourceUrl", per_process::metadata.release.source_url);
  READONLY_STRING_PROPERTY(
      release, "headersUrl", per_process::metadata.release.headers_url);
#ifdef _WIN32
  READONLY_STRING_PROPERTY(
      release, "libUrl", per_process::metadata.release.lib_url);
#endif  // _WIN32
#endif  // NODE_HAS_RELEASE_URLS

  // process._rawDebug: may be overwritten later in JS land, but should be
  // available from the beginning for debugging purposes
  env->SetMethod(process, "_rawDebug", RawDebug);

  return scope.Escape(process);
}

void PatchProcessObject(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  CHECK(args[0]->IsObject());
  Local<Object> process = args[0].As<Object>();

  // process.title
  CHECK(process
            ->SetAccessor(
                context,
                FIXED_ONE_BYTE_STRING(isolate, "title"),
                ProcessTitleGetter,
                env->owns_process_state() ? ProcessTitleSetter : nullptr,
                env->current_callback_data(),
                DEFAULT,
                None,
                SideEffectType::kHasNoSideEffect)
            .FromJust());

  // process.argv
  process->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "argv"),
               ToV8Value(context, env->argv()).ToLocalChecked()).Check();

  // process.execArgv
  process->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "execArgv"),
               ToV8Value(context, env->exec_argv())
                   .ToLocalChecked()).Check();

  READONLY_PROPERTY(process, "pid",
                    Integer::New(isolate, uv_os_getpid()));

  CHECK(process->SetAccessor(context,
                             FIXED_ONE_BYTE_STRING(isolate, "ppid"),
                             GetParentProcessId).FromJust());

  // --security-revert flags
#define V(code, _, __)                                                        \
  do {                                                                        \
    if (IsReverted(SECURITY_REVERT_ ## code)) {                               \
      READONLY_PROPERTY(process, "REVERT_" #code, True(isolate));             \
    }                                                                         \
  } while (0);
  SECURITY_REVERSIONS(V)
#undef V

  // process.execPath
  process
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "execPath"),
            String::NewFromUtf8(isolate,
                                env->exec_path().c_str(),
                                NewStringType::kInternalized,
                                env->exec_path().size())
                .ToLocalChecked())
      .Check();

  // process.debugPort
  CHECK(process
            ->SetAccessor(context,
                          FIXED_ONE_BYTE_STRING(isolate, "debugPort"),
                          DebugPortGetter,
                          env->owns_process_state() ? DebugPortSetter : nullptr,
                          env->current_callback_data())
            .FromJust());
}

}  // namespace node

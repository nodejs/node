#include <limits.h>  // PATH_MAX

#include "env-inl.h"
#include "node_internals.h"
#include "node_options-inl.h"
#include "node_metadata.h"
#include "node_revert.h"
#include "util-inl.h"

namespace node {
using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::NewStringType;
using v8::None;
using v8::Object;
using v8::SideEffectType;
using v8::String;
using v8::Value;

Local<Object> CreateProcessObject(Environment* env,
                                  const std::vector<std::string>& args,
                                  const std::vector<std::string>& exec_args) {
  Isolate* isolate = env->isolate();
  EscapableHandleScope scope(isolate);
  Local<Context> context = env->context();

  Local<FunctionTemplate> process_template = FunctionTemplate::New(isolate);
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "process"));
  Local<Object> process = process_template->GetFunction(context)
                              .ToLocalChecked()
                              ->NewInstance(context)
                              .ToLocalChecked();

  auto title_string = FIXED_ONE_BYTE_STRING(env->isolate(), "title");
  CHECK(process->SetAccessor(
      env->context(),
      title_string,
      ProcessTitleGetter,
      env->is_main_thread() ? ProcessTitleSetter : nullptr,
      env->as_external(),
      DEFAULT,
      None,
      SideEffectType::kHasNoSideEffect).FromJust());

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

  // process.argv
  process->Set(env->context(),
               FIXED_ONE_BYTE_STRING(env->isolate(), "argv"),
               ToV8Value(env->context(), args).ToLocalChecked()).FromJust();

  // process.execArgv
  process->Set(env->context(),
               FIXED_ONE_BYTE_STRING(env->isolate(), "execArgv"),
               ToV8Value(env->context(), exec_args)
                   .ToLocalChecked()).FromJust();

  // create process.env
  process
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "env"),
            CreateEnvVarProxy(context, isolate, env->as_external()))
      .FromJust();

  READONLY_PROPERTY(process, "pid",
                    Integer::New(env->isolate(), uv_os_getpid()));

  CHECK(process->SetAccessor(env->context(),
                             FIXED_ONE_BYTE_STRING(env->isolate(), "ppid"),
                             GetParentProcessId).FromJust());

  // -e, --eval
  // TODO(addaleax): Remove this.
  if (env->options()->has_eval_string) {
    READONLY_PROPERTY(process,
                      "_eval",
                      String::NewFromUtf8(
                          env->isolate(),
                          env->options()->eval_string.c_str(),
                          NewStringType::kNormal).ToLocalChecked());
  }

  // -p, --print
  // TODO(addaleax): Remove this.
  if (env->options()->print_eval) {
    READONLY_PROPERTY(process, "_print_eval", True(env->isolate()));
  }

  // -c, --check
  // TODO(addaleax): Remove this.
  if (env->options()->syntax_check_only) {
    READONLY_PROPERTY(process, "_syntax_check_only", True(env->isolate()));
  }

  // -i, --interactive
  // TODO(addaleax): Remove this.
  if (env->options()->force_repl) {
    READONLY_PROPERTY(process, "_forceRepl", True(env->isolate()));
  }

  // -r, --require
  // TODO(addaleax): Remove this.
  const std::vector<std::string>& preload_modules =
      env->options()->preload_modules;
  if (!preload_modules.empty()) {
    READONLY_PROPERTY(process,
                      "_preload_modules",
                      ToV8Value(env->context(), preload_modules)
                          .ToLocalChecked());
  }

  // --no-deprecation
  if (env->options()->no_deprecation) {
    READONLY_PROPERTY(process, "noDeprecation", True(env->isolate()));
  }

  // --no-warnings
  if (env->options()->no_warnings) {
    READONLY_PROPERTY(process, "noProcessWarnings", True(env->isolate()));
  }

  // --trace-warnings
  if (env->options()->trace_warnings) {
    READONLY_PROPERTY(process, "traceProcessWarnings", True(env->isolate()));
  }

  // --throw-deprecation
  if (env->options()->throw_deprecation) {
    READONLY_PROPERTY(process, "throwDeprecation", True(env->isolate()));
  }

#ifdef NODE_NO_BROWSER_GLOBALS
  // configure --no-browser-globals
  READONLY_PROPERTY(process, "_noBrowserGlobals", True(env->isolate()));
#endif  // NODE_NO_BROWSER_GLOBALS

  // --prof-process
  // TODO(addaleax): Remove this.
  if (env->options()->prof_process) {
    READONLY_PROPERTY(process, "profProcess", True(env->isolate()));
  }

  // --trace-deprecation
  if (env->options()->trace_deprecation) {
    READONLY_PROPERTY(process, "traceDeprecation", True(env->isolate()));
  }

  // TODO(refack): move the following 4 to `node_config`
  // --inspect-brk
  if (env->options()->debug_options().wait_for_connect()) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_breakFirstLine", True(env->isolate()));
  }

  if (env->options()->debug_options().break_node_first_line) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_breakNodeFirstLine", True(env->isolate()));
  }

  // --inspect --debug-brk
  if (env->options()->debug_options().deprecated_invocation()) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_deprecatedDebugBrk", True(env->isolate()));
  }

  // --debug or, --debug-brk without --inspect
  if (env->options()->debug_options().invalid_invocation()) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_invalidDebug", True(env->isolate()));
  }

  // --security-revert flags
#define V(code, _, __)                                                        \
  do {                                                                        \
    if (IsReverted(SECURITY_REVERT_ ## code)) {                               \
      READONLY_PROPERTY(process, "REVERT_" #code, True(env->isolate()));      \
    }                                                                         \
  } while (0);
  SECURITY_REVERSIONS(V)
#undef V

  {
    size_t exec_path_len = 2 * PATH_MAX;
    std::vector<char> exec_path(exec_path_len);
    Local<String> exec_path_value;
    if (uv_exepath(exec_path.data(), &exec_path_len) == 0) {
      exec_path_value = String::NewFromUtf8(env->isolate(),
                                            exec_path.data(),
                                            NewStringType::kInternalized,
                                            exec_path_len).ToLocalChecked();
    } else {
      exec_path_value = String::NewFromUtf8(env->isolate(), args[0].c_str(),
          NewStringType::kInternalized).ToLocalChecked();
    }
    process->Set(env->context(),
                 FIXED_ONE_BYTE_STRING(env->isolate(), "execPath"),
                 exec_path_value).FromJust();
  }

  auto debug_port_string = FIXED_ONE_BYTE_STRING(env->isolate(), "debugPort");
  CHECK(process->SetAccessor(env->context(),
                             debug_port_string,
                             DebugPortGetter,
                             env->is_main_thread() ? DebugPortSetter : nullptr,
                             env->as_external()).FromJust());

  return scope.Escape(process);
}

}  // namespace node

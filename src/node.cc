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

#include "node_buffer.h"
#include "node_constants.h"
#include "node_javascript.h"
#include "node_code_cache.h"
#include "node_platform.h"
#include "node_version.h"
#include "node_internals.h"
#include "node_revert.h"
#include "node_perf.h"
#include "node_context_data.h"
#include "tracing/traced_value.h"

#if defined HAVE_PERFCTR
#include "node_counters.h"
#endif

#if HAVE_OPENSSL
#include "node_crypto.h"
#endif

#if defined(NODE_HAVE_I18N_SUPPORT)
#include "node_i18n.h"
#endif

#if HAVE_INSPECTOR
#include "inspector_io.h"
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
#include "node_dtrace.h"
#endif

#include "ares.h"
#include "async_wrap-inl.h"
#include "brotli/encode.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "http_parser.h"
#include "nghttp2/nghttp2ver.h"
#include "req_wrap-inl.h"
#include "string_bytes.h"
#include "tracing/agent.h"
#include "tracing/node_trace_writer.h"
#include "util.h"
#include "uv.h"
#if NODE_USE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif  // NODE_USE_V8_PLATFORM
#include "v8-profiler.h"
#include "zlib.h"

#ifdef NODE_ENABLE_VTUNE_PROFILING
#include "../deps/v8/src/third_party/vtune/v8-vtune.h"
#endif

#ifdef NODE_ENABLE_LARGE_CODE_PAGES
#include "large_pages/node_large_page.h"
#endif

#include <errno.h>
#include <fcntl.h>  // _O_RDWR
#include <limits.h>  // PATH_MAX
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <vector>

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/uvernum.h>
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#define umask _umask
typedef int mode_t;
#else
#include <pthread.h>
#include <sys/resource.h>  // getrlimit, setrlimit
#include <unistd.h>  // setuid, getuid
#endif

#if defined(__POSIX__) && !defined(__ANDROID__) && !defined(__CloudABI__)
#include <pwd.h>  // getpwnam()
#include <grp.h>  // getgrnam()
#endif

#if defined(__POSIX__)
#include <dlfcn.h>
#endif

// This is used to load built-in modules. Instead of using
// __attribute__((constructor)), we call the _register_<modname>
// function for each built-in modules explicitly in
// node::RegisterBuiltinModules(). This is only forward declaration.
// The definitions are in each module's implementation when calling
// the NODE_BUILTIN_MODULE_CONTEXT_AWARE.
#define V(modname) void _register_##modname();
  NODE_BUILTIN_MODULES(V)
#undef V

namespace node {

using options_parser::kAllowedInEnvironment;
using options_parser::kDisallowedInEnvironment;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::DEFAULT;
using v8::DontEnum;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Locker;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Message;
using v8::MicrotasksPolicy;
using v8::NamedPropertyHandlerConfiguration;
using v8::NewStringType;
using v8::None;
using v8::Nothing;
using v8::Null;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::SealHandleScope;
using v8::SideEffectType;
using v8::String;
using v8::TracingController;
using v8::TryCatch;
using v8::Undefined;
using v8::V8;
using v8::Value;

static bool v8_is_profiling = false;
static bool node_is_initialized = false;
static uv_once_t init_modpending_once = UV_ONCE_INIT;
static uv_key_t thread_local_modpending;
static node_module* modlist_builtin;
static node_module* modlist_internal;
static node_module* modlist_linked;
static node_module* modlist_addon;

// Bit flag used to track security reverts (see node_revert.h)
unsigned int reverted = 0;

bool v8_initialized = false;

bool linux_at_secure = false;

// process-relative uptime base, initialized at start-up
double prog_start_time;

Mutex per_process_opts_mutex;
std::shared_ptr<PerProcessOptions> per_process_opts {
    new PerProcessOptions() };

static Mutex node_isolate_mutex;
static Isolate* node_isolate;

// Ensures that __metadata trace events are only emitted
// when tracing is enabled.
class NodeTraceStateObserver :
    public TracingController::TraceStateObserver {
 public:
  void OnTraceEnabled() override {
    char name_buffer[512];
    if (uv_get_process_title(name_buffer, sizeof(name_buffer)) == 0) {
      // Only emit the metadata event if the title can be retrieved
      // successfully. Ignore it otherwise.
      TRACE_EVENT_METADATA1("__metadata", "process_name",
                            "name", TRACE_STR_COPY(name_buffer));
    }
    TRACE_EVENT_METADATA1("__metadata", "version",
                          "node", NODE_VERSION_STRING);
    TRACE_EVENT_METADATA1("__metadata", "thread_name",
                          "name", "JavaScriptMainThread");

    auto trace_process = tracing::TracedValue::Create();
    trace_process->BeginDictionary("versions");

    const char http_parser_version[] =
        NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
        "."
        NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)
        "."
        NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);

    const char node_napi_version[] = NODE_STRINGIFY(NAPI_VERSION);
    const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);

    trace_process->SetString("http_parser", http_parser_version);
    trace_process->SetString("node", NODE_VERSION_STRING);
    trace_process->SetString("v8", V8::GetVersion());
    trace_process->SetString("uv", uv_version_string());
    trace_process->SetString("zlib", ZLIB_VERSION);
    trace_process->SetString("ares", ARES_VERSION_STR);
    trace_process->SetString("modules", node_modules_version);
    trace_process->SetString("nghttp2", NGHTTP2_VERSION);
    trace_process->SetString("napi", node_napi_version);

#if HAVE_OPENSSL
    trace_process->SetString("openssl", crypto::GetOpenSSLVersion());
#endif
    trace_process->EndDictionary();

    trace_process->SetString("arch", NODE_ARCH);
    trace_process->SetString("platform", NODE_PLATFORM);

    trace_process->BeginDictionary("release");
    trace_process->SetString("name", NODE_RELEASE);
#if NODE_VERSION_IS_LTS
    trace_process->SetString("lts", NODE_VERSION_LTS_CODENAME);
#endif
    trace_process->EndDictionary();
    TRACE_EVENT_METADATA1("__metadata", "node",
                          "process", std::move(trace_process));

    // This only runs the first time tracing is enabled
    controller_->RemoveTraceStateObserver(this);
  }

  void OnTraceDisabled() override {
    // Do nothing here. This should never be called because the
    // observer removes itself when OnTraceEnabled() is called.
    UNREACHABLE();
  }

  explicit NodeTraceStateObserver(TracingController* controller) :
      controller_(controller) {}
  ~NodeTraceStateObserver() override {}

 private:
  TracingController* controller_;
};

static struct {
#if NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {
    tracing_agent_.reset(new tracing::Agent());
    node::tracing::TraceEventHelper::SetAgent(tracing_agent_.get());
    node::tracing::TracingController* controller =
        tracing_agent_->GetTracingController();
    trace_state_observer_.reset(new NodeTraceStateObserver(controller));
    controller->AddTraceStateObserver(trace_state_observer_.get());
    StartTracingAgent();
    // Tracing must be initialized before platform threads are created.
    platform_ = new NodePlatform(thread_pool_size, controller);
    V8::InitializePlatform(platform_);
  }

  void Dispose() {
    platform_->Shutdown();
    delete platform_;
    platform_ = nullptr;
    // Destroy tracing after the platform (and platform threads) have been
    // stopped.
    tracing_agent_.reset(nullptr);
    trace_state_observer_.reset(nullptr);
  }

  void DrainVMTasks(Isolate* isolate) {
    platform_->DrainBackgroundTasks(isolate);
  }

  void CancelVMTasks(Isolate* isolate) {
    platform_->CancelPendingDelayedTasks(isolate);
  }

#if HAVE_INSPECTOR
  bool StartInspector(Environment* env, const char* script_path,
                      std::shared_ptr<DebugOptions> options) {
    // Inspector agent can't fail to start, but if it was configured to listen
    // right away on the websocket port and fails to bind/etc, this will return
    // false.
    return env->inspector_agent()->Start(
        script_path == nullptr ? "" : script_path, options, true);
  }

  bool InspectorStarted(Environment* env) {
    return env->inspector_agent()->IsListening();
  }
#endif  // HAVE_INSPECTOR

  void StartTracingAgent() {
    if (per_process_opts->trace_event_categories.empty()) {
      tracing_file_writer_ = tracing_agent_->DefaultHandle();
    } else {
      tracing_file_writer_ = tracing_agent_->AddClient(
          ParseCommaSeparatedSet(per_process_opts->trace_event_categories),
          std::unique_ptr<tracing::AsyncTraceWriter>(
              new tracing::NodeTraceWriter(
                  per_process_opts->trace_event_file_pattern)),
          tracing::Agent::kUseDefaultCategories);
    }
  }

  void StopTracingAgent() {
    tracing_file_writer_.reset();
  }

  tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return &tracing_file_writer_;
  }

  NodePlatform* Platform() {
    return platform_;
  }

  std::unique_ptr<NodeTraceStateObserver> trace_state_observer_;
  std::unique_ptr<tracing::Agent> tracing_agent_;
  tracing::AgentWriterHandle tracing_file_writer_;
  NodePlatform* platform_;
#else  // !NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {}
  void Dispose() {}
  void DrainVMTasks(Isolate* isolate) {}
  void CancelVMTasks(Isolate* isolate) {}
  bool StartInspector(Environment* env, const char* script_path,
                      const DebugOptions& options) {
    env->ThrowError("Node compiled with NODE_USE_V8_PLATFORM=0");
    return true;
  }

  void StartTracingAgent() {
    if (!trace_enabled_categories.empty()) {
      fprintf(stderr, "Node compiled with NODE_USE_V8_PLATFORM=0, "
                      "so event tracing is not available.\n");
    }
  }
  void StopTracingAgent() {}

  tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return nullptr;
  }

  NodePlatform* Platform() {
    return nullptr;
  }
#endif  // !NODE_USE_V8_PLATFORM

#if !NODE_USE_V8_PLATFORM || !HAVE_INSPECTOR
  bool InspectorStarted(Environment* env) {
    return false;
  }
#endif  //  !NODE_USE_V8_PLATFORM || !HAVE_INSPECTOR
} v8_platform;

void DisposePlatform() {
  v8_platform.Dispose();
}

#ifdef __POSIX__
static const unsigned kMaxSignal = 32;
#endif

void PrintErrorString(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
#ifdef _WIN32
  HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);

  // Check if stderr is something other than a tty/console
  if (stderr_handle == INVALID_HANDLE_VALUE ||
      stderr_handle == nullptr ||
      uv_guess_handle(_fileno(stderr)) != UV_TTY) {
    vfprintf(stderr, format, ap);
    va_end(ap);
    return;
  }

  // Fill in any placeholders
  int n = _vscprintf(format, ap);
  std::vector<char> out(n + 1);
  vsprintf(out.data(), format, ap);

  // Get required wide buffer size
  n = MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, nullptr, 0);

  std::vector<wchar_t> wbuf(n);
  MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, wbuf.data(), n);

  // Don't include the null character in the output
  CHECK_GT(n, 0);
  WriteConsoleW(stderr_handle, wbuf.data(), n - 1, nullptr, nullptr);
#else
  vfprintf(stderr, format, ap);
#endif
  va_end(ap);
}

const char* signo_string(int signo) {
#define SIGNO_CASE(e)  case e: return #e;
  switch (signo) {
#ifdef SIGHUP
  SIGNO_CASE(SIGHUP);
#endif

#ifdef SIGINT
  SIGNO_CASE(SIGINT);
#endif

#ifdef SIGQUIT
  SIGNO_CASE(SIGQUIT);
#endif

#ifdef SIGILL
  SIGNO_CASE(SIGILL);
#endif

#ifdef SIGTRAP
  SIGNO_CASE(SIGTRAP);
#endif

#ifdef SIGABRT
  SIGNO_CASE(SIGABRT);
#endif

#ifdef SIGIOT
# if SIGABRT != SIGIOT
  SIGNO_CASE(SIGIOT);
# endif
#endif

#ifdef SIGBUS
  SIGNO_CASE(SIGBUS);
#endif

#ifdef SIGFPE
  SIGNO_CASE(SIGFPE);
#endif

#ifdef SIGKILL
  SIGNO_CASE(SIGKILL);
#endif

#ifdef SIGUSR1
  SIGNO_CASE(SIGUSR1);
#endif

#ifdef SIGSEGV
  SIGNO_CASE(SIGSEGV);
#endif

#ifdef SIGUSR2
  SIGNO_CASE(SIGUSR2);
#endif

#ifdef SIGPIPE
  SIGNO_CASE(SIGPIPE);
#endif

#ifdef SIGALRM
  SIGNO_CASE(SIGALRM);
#endif

  SIGNO_CASE(SIGTERM);

#ifdef SIGCHLD
  SIGNO_CASE(SIGCHLD);
#endif

#ifdef SIGSTKFLT
  SIGNO_CASE(SIGSTKFLT);
#endif


#ifdef SIGCONT
  SIGNO_CASE(SIGCONT);
#endif

#ifdef SIGSTOP
  SIGNO_CASE(SIGSTOP);
#endif

#ifdef SIGTSTP
  SIGNO_CASE(SIGTSTP);
#endif

#ifdef SIGBREAK
  SIGNO_CASE(SIGBREAK);
#endif

#ifdef SIGTTIN
  SIGNO_CASE(SIGTTIN);
#endif

#ifdef SIGTTOU
  SIGNO_CASE(SIGTTOU);
#endif

#ifdef SIGURG
  SIGNO_CASE(SIGURG);
#endif

#ifdef SIGXCPU
  SIGNO_CASE(SIGXCPU);
#endif

#ifdef SIGXFSZ
  SIGNO_CASE(SIGXFSZ);
#endif

#ifdef SIGVTALRM
  SIGNO_CASE(SIGVTALRM);
#endif

#ifdef SIGPROF
  SIGNO_CASE(SIGPROF);
#endif

#ifdef SIGWINCH
  SIGNO_CASE(SIGWINCH);
#endif

#ifdef SIGIO
  SIGNO_CASE(SIGIO);
#endif

#ifdef SIGPOLL
# if SIGPOLL != SIGIO
  SIGNO_CASE(SIGPOLL);
# endif
#endif

#ifdef SIGLOST
# if SIGLOST != SIGABRT
  SIGNO_CASE(SIGLOST);
# endif
#endif

#ifdef SIGPWR
# if SIGPWR != SIGLOST
  SIGNO_CASE(SIGPWR);
# endif
#endif

#ifdef SIGINFO
# if !defined(SIGPWR) || SIGINFO != SIGPWR
  SIGNO_CASE(SIGINFO);
# endif
#endif

#ifdef SIGSYS
  SIGNO_CASE(SIGSYS);
#endif

  default: return "";
  }
}

// Look up environment variable unless running as setuid root.
bool SafeGetenv(const char* key, std::string* text) {
#if !defined(__CloudABI__) && !defined(_WIN32)
  if (linux_at_secure || getuid() != geteuid() || getgid() != getegid())
    goto fail;
#endif

  {
    Mutex::ScopedLock lock(environ_mutex);
    if (const char* value = getenv(key)) {
      *text = value;
      return true;
    }
  }

fail:
  text->clear();
  return false;
}


void* ArrayBufferAllocator::Allocate(size_t size) {
  if (zero_fill_field_ || zero_fill_all_buffers)
    return UncheckedCalloc(size);
  else
    return UncheckedMalloc(size);
}

namespace {

bool ShouldAbortOnUncaughtException(Isolate* isolate) {
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  return env != nullptr &&
         env->should_abort_on_uncaught_toggle()[0] &&
         !env->inside_should_not_abort_on_uncaught_scope();
}

}  // anonymous namespace


void AddPromiseHook(Isolate* isolate, promise_hook_func fn, void* arg) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  env->AddPromiseHook(fn, arg);
}

void AddEnvironmentCleanupHook(Isolate* isolate,
                               void (*fun)(void* arg),
                               void* arg) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  env->AddCleanupHook(fun, arg);
}


void RemoveEnvironmentCleanupHook(Isolate* isolate,
                                  void (*fun)(void* arg),
                                  void* arg) {
  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);
  env->RemoveCleanupHook(fun, arg);
}

MaybeLocal<Value> InternalMakeCallback(Environment* env,
                                       Local<Object> recv,
                                       const Local<Function> callback,
                                       int argc,
                                       Local<Value> argv[],
                                       async_context asyncContext) {
  CHECK(!recv.IsEmpty());
  InternalCallbackScope scope(env, recv, asyncContext);
  if (scope.Failed()) {
    return MaybeLocal<Value>();
  }

  Local<Function> domain_cb = env->domain_callback();
  MaybeLocal<Value> ret;
  if (asyncContext.async_id != 0 || domain_cb.IsEmpty() || recv.IsEmpty()) {
    ret = callback->Call(env->context(), recv, argc, argv);
  } else {
    std::vector<Local<Value>> args(1 + argc);
    args[0] = callback;
    std::copy(&argv[0], &argv[argc], args.begin() + 1);
    ret = domain_cb->Call(env->context(), recv, args.size(), &args[0]);
  }

  if (ret.IsEmpty()) {
    scope.MarkAsFailed();
    return MaybeLocal<Value>();
  }

  scope.Close();
  if (scope.Failed()) {
    return MaybeLocal<Value>();
  }

  return ret;
}


// Public MakeCallback()s


MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               const char* method,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  Local<String> method_string =
      String::NewFromUtf8(isolate, method, NewStringType::kNormal)
          .ToLocalChecked();
  return MakeCallback(isolate, recv, method_string, argc, argv, asyncContext);
}


MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               Local<String> symbol,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  Local<Value> callback_v = recv->Get(symbol);
  if (callback_v.IsEmpty()) return Local<Value>();
  if (!callback_v->IsFunction()) return Local<Value>();
  Local<Function> callback = callback_v.As<Function>();
  return MakeCallback(isolate, recv, callback, argc, argv, asyncContext);
}


MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               Local<Function> callback,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  // Observe the following two subtleties:
  //
  // 1. The environment is retrieved from the callback function's context.
  // 2. The context to enter is retrieved from the environment.
  //
  // Because of the AssignToContext() call in src/node_contextify.cc,
  // the two contexts need not be the same.
  Environment* env = Environment::GetCurrent(callback->CreationContext());
  CHECK_NOT_NULL(env);
  Context::Scope context_scope(env->context());
  MaybeLocal<Value> ret = InternalMakeCallback(env, recv, callback,
                                               argc, argv, asyncContext);
  if (ret.IsEmpty() && env->makecallback_depth() == 0) {
    // This is only for legacy compatiblity and we may want to look into
    // removing/adjusting it.
    return Undefined(env->isolate());
  }
  return ret;
}


// Legacy MakeCallback()s

Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          const char* method,
                          int argc,
                          Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, method, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}


Local<Value> MakeCallback(Isolate* isolate,
    Local<Object> recv,
    Local<String> symbol,
    int argc,
    Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, symbol, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}


Local<Value> MakeCallback(Isolate* isolate,
    Local<Object> recv,
    Local<Function> callback,
    int argc,
    Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, callback, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}

bool IsExceptionDecorated(Environment* env, Local<Value> er) {
  if (!er.IsEmpty() && er->IsObject()) {
    Local<Object> err_obj = er.As<Object>();
    auto maybe_value =
        err_obj->GetPrivate(env->context(), env->decorated_private_symbol());
    Local<Value> decorated;
    return maybe_value.ToLocal(&decorated) && decorated->IsTrue();
  }
  return false;
}

void AppendExceptionLine(Environment* env,
                         Local<Value> er,
                         Local<Message> message,
                         enum ErrorHandlingMode mode) {
  if (message.IsEmpty())
    return;

  HandleScope scope(env->isolate());
  Local<Object> err_obj;
  if (!er.IsEmpty() && er->IsObject()) {
    err_obj = er.As<Object>();
  }

  // Print (filename):(line number): (message).
  ScriptOrigin origin = message->GetScriptOrigin();
  node::Utf8Value filename(env->isolate(), message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber(env->context()).FromJust();
  // Print line of source code.
  MaybeLocal<String> source_line_maybe = message->GetSourceLine(env->context());
  node::Utf8Value sourceline(env->isolate(),
                             source_line_maybe.ToLocalChecked());
  const char* sourceline_string = *sourceline;
  if (strstr(sourceline_string, "node-do-not-add-exception-line") != nullptr)
    return;

  // Because of how node modules work, all scripts are wrapped with a
  // "function (module, exports, __filename, ...) {"
  // to provide script local variables.
  //
  // When reporting errors on the first line of a script, this wrapper
  // function is leaked to the user. There used to be a hack here to
  // truncate off the first 62 characters, but it caused numerous other
  // problems when vm.runIn*Context() methods were used for non-module
  // code.
  //
  // If we ever decide to re-instate such a hack, the following steps
  // must be taken:
  //
  // 1. Pass a flag around to say "this code was wrapped"
  // 2. Update the stack frame output so that it is also correct.
  //
  // It would probably be simpler to add a line rather than add some
  // number of characters to the first line, since V8 truncates the
  // sourceline to 78 characters, and we end up not providing very much
  // useful debugging info to the user if we remove 62 characters.

  int script_start =
      (linenum - origin.ResourceLineOffset()->Value()) == 1 ?
          origin.ResourceColumnOffset()->Value() : 0;
  int start = message->GetStartColumn(env->context()).FromMaybe(0);
  int end = message->GetEndColumn(env->context()).FromMaybe(0);
  if (start >= script_start) {
    CHECK_GE(end, start);
    start -= script_start;
    end -= script_start;
  }

  char arrow[1024];
  int max_off = sizeof(arrow) - 2;

  int off = snprintf(arrow,
                     sizeof(arrow),
                     "%s:%i\n%s\n",
                     filename_string,
                     linenum,
                     sourceline_string);
  CHECK_GE(off, 0);
  if (off > max_off) {
    off = max_off;
  }

  // Print wavy underline (GetUnderline is deprecated).
  for (int i = 0; i < start; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = (sourceline_string[i] == '\t') ? '\t' : ' ';
  }
  for (int i = start; i < end; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = '^';
  }
  CHECK_LE(off, max_off);
  arrow[off] = '\n';
  arrow[off + 1] = '\0';

  Local<String> arrow_str = String::NewFromUtf8(env->isolate(), arrow,
      NewStringType::kNormal).ToLocalChecked();

  const bool can_set_arrow = !arrow_str.IsEmpty() && !err_obj.IsEmpty();
  // If allocating arrow_str failed, print it out. There's not much else to do.
  // If it's not an error, but something needs to be printed out because
  // it's a fatal exception, also print it out from here.
  // Otherwise, the arrow property will be attached to the object and handled
  // by the caller.
  if (!can_set_arrow || (mode == FATAL_ERROR && !err_obj->IsNativeError())) {
    if (env->printed_error())
      return;
    Mutex::ScopedLock lock(process_mutex);
    env->set_printed_error(true);

    uv_tty_reset_mode();
    PrintErrorString("\n%s", arrow);
    return;
  }

  CHECK(err_obj->SetPrivate(
            env->context(),
            env->arrow_message_private_symbol(),
            arrow_str).FromMaybe(false));
}


void ReportException(Environment* env,
                     Local<Value> er,
                     Local<Message> message) {
  CHECK(!er.IsEmpty());
  HandleScope scope(env->isolate());

  if (message.IsEmpty())
    message = Exception::CreateMessage(env->isolate(), er);

  AppendExceptionLine(env, er, message, FATAL_ERROR);

  Local<Value> trace_value;
  Local<Value> arrow;
  const bool decorated = IsExceptionDecorated(env, er);

  if (er->IsUndefined() || er->IsNull()) {
    trace_value = Undefined(env->isolate());
  } else {
    Local<Object> err_obj = er->ToObject(env->context()).ToLocalChecked();

    trace_value = err_obj->Get(env->stack_string());
    arrow =
        err_obj->GetPrivate(
            env->context(),
            env->arrow_message_private_symbol()).ToLocalChecked();
  }

  node::Utf8Value trace(env->isolate(), trace_value);

  // range errors have a trace member set to undefined
  if (trace.length() > 0 && !trace_value->IsUndefined()) {
    if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
      PrintErrorString("%s\n", *trace);
    } else {
      node::Utf8Value arrow_string(env->isolate(), arrow);
      PrintErrorString("%s\n%s\n", *arrow_string, *trace);
    }
  } else {
    // this really only happens for RangeErrors, since they're the only
    // kind that won't have all this info in the trace, or when non-Error
    // objects are thrown manually.
    Local<Value> message;
    Local<Value> name;

    if (er->IsObject()) {
      Local<Object> err_obj = er.As<Object>();
      message = err_obj->Get(env->message_string());
      name = err_obj->Get(FIXED_ONE_BYTE_STRING(env->isolate(), "name"));
    }

    if (message.IsEmpty() ||
        message->IsUndefined() ||
        name.IsEmpty() ||
        name->IsUndefined()) {
      // Not an error object. Just print as-is.
      String::Utf8Value message(env->isolate(), er);

      PrintErrorString("%s\n", *message ? *message :
                                          "<toString() threw exception>");
    } else {
      node::Utf8Value name_string(env->isolate(), name);
      node::Utf8Value message_string(env->isolate(), message);

      if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
        PrintErrorString("%s: %s\n", *name_string, *message_string);
      } else {
        node::Utf8Value arrow_string(env->isolate(), arrow);
        PrintErrorString("%s\n%s: %s\n",
                         *arrow_string,
                         *name_string,
                         *message_string);
      }
    }
  }

  fflush(stderr);

#if HAVE_INSPECTOR
  env->inspector_agent()->FatalException(er, message);
#endif
}


static void ReportException(Environment* env, const TryCatch& try_catch) {
  ReportException(env, try_catch.Exception(), try_catch.Message());
}


// Executes a str within the current v8 context.
static MaybeLocal<Value> ExecuteString(Environment* env,
                                       Local<String> source,
                                       Local<String> filename) {
  EscapableHandleScope scope(env->isolate());
  TryCatch try_catch(env->isolate());

  // try_catch must be nonverbose to disable FatalException() handler,
  // we will handle exceptions ourself.
  try_catch.SetVerbose(false);

  ScriptOrigin origin(filename);
  MaybeLocal<Script> script =
      Script::Compile(env->context(), source, &origin);
  if (script.IsEmpty()) {
    ReportException(env, try_catch);
    env->Exit(3);
    return MaybeLocal<Value>();
  }

  MaybeLocal<Value> result = script.ToLocalChecked()->Run(env->context());
  if (result.IsEmpty()) {
    if (try_catch.HasTerminated()) {
      env->isolate()->CancelTerminateExecution();
      return MaybeLocal<Value>();
    }
    ReportException(env, try_catch);
    env->Exit(4);
    return MaybeLocal<Value>();
  }

  return scope.Escape(result.ToLocalChecked());
}


[[noreturn]] void Abort() {
  DumpBacktrace(stderr);
  fflush(stderr);
  ABORT_NO_BACKTRACE();
}


[[noreturn]] void Assert(const char* const (*args)[4]) {
  auto filename = (*args)[0];
  auto linenum = (*args)[1];
  auto message = (*args)[2];
  auto function = (*args)[3];

  char name[1024];
  GetHumanReadableProcessName(&name);

  fprintf(stderr, "%s: %s:%s:%s%s Assertion `%s' failed.\n",
          name, filename, linenum, function, *function ? ":" : "", message);
  fflush(stderr);

  Abort();
}

static void WaitForInspectorDisconnect(Environment* env) {
#if HAVE_INSPECTOR
  if (env->inspector_agent()->IsActive()) {
    // Restore signal dispositions, the app is done and is no longer
    // capable of handling signals.
#if defined(__POSIX__) && !defined(NODE_SHARED_MODE)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    for (unsigned nr = 1; nr < kMaxSignal; nr += 1) {
      if (nr == SIGKILL || nr == SIGSTOP || nr == SIGPROF)
        continue;
      act.sa_handler = (nr == SIGPIPE) ? SIG_IGN : SIG_DFL;
      CHECK_EQ(0, sigaction(nr, &act, nullptr));
    }
#endif
    env->inspector_agent()->WaitForDisconnect();
  }
#endif
}


static void Exit(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  WaitForInspectorDisconnect(env);
  v8_platform.StopTracingAgent();
  int code = args[0]->Int32Value(env->context()).FromMaybe(0);
  env->Exit(code);
}

extern "C" void node_module_register(void* m) {
  struct node_module* mp = reinterpret_cast<struct node_module*>(m);

  if (mp->nm_flags & NM_F_BUILTIN) {
    mp->nm_link = modlist_builtin;
    modlist_builtin = mp;
  } else if (mp->nm_flags & NM_F_INTERNAL) {
    mp->nm_link = modlist_internal;
    modlist_internal = mp;
  } else if (!node_is_initialized) {
    // "Linked" modules are included as part of the node project.
    // Like builtins they are registered *before* node::Init runs.
    mp->nm_flags = NM_F_LINKED;
    mp->nm_link = modlist_linked;
    modlist_linked = mp;
  } else {
    uv_key_set(&thread_local_modpending, mp);
  }
}

inline struct node_module* FindModule(struct node_module* list,
                                      const char* name,
                                      int flag) {
  struct node_module* mp;

  for (mp = list; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & flag) != 0);
  return mp;
}

node_module* get_builtin_module(const char* name) {
  return FindModule(modlist_builtin, name, NM_F_BUILTIN);
}
node_module* get_internal_module(const char* name) {
  return FindModule(modlist_internal, name, NM_F_INTERNAL);
}
node_module* get_linked_module(const char* name) {
  return FindModule(modlist_linked, name, NM_F_LINKED);
}

class DLib {
 public:
#ifdef __POSIX__
  static const int kDefaultFlags = RTLD_LAZY;
#else
  static const int kDefaultFlags = 0;
#endif

  inline DLib(const char* filename, int flags)
      : filename_(filename), flags_(flags), handle_(nullptr) {}

  inline bool Open();
  inline void Close();
  inline void* GetSymbolAddress(const char* name);

  const std::string filename_;
  const int flags_;
  std::string errmsg_;
  void* handle_;
#ifndef __POSIX__
  uv_lib_t lib_;
#endif
 private:
  DISALLOW_COPY_AND_ASSIGN(DLib);
};


#ifdef __POSIX__
bool DLib::Open() {
  handle_ = dlopen(filename_.c_str(), flags_);
  if (handle_ != nullptr)
    return true;
  errmsg_ = dlerror();
  return false;
}

void DLib::Close() {
  if (handle_ == nullptr) return;
  dlclose(handle_);
  handle_ = nullptr;
}

void* DLib::GetSymbolAddress(const char* name) {
  return dlsym(handle_, name);
}
#else  // !__POSIX__
bool DLib::Open() {
  int ret = uv_dlopen(filename_.c_str(), &lib_);
  if (ret == 0) {
    handle_ = static_cast<void*>(lib_.handle);
    return true;
  }
  errmsg_ = uv_dlerror(&lib_);
  uv_dlclose(&lib_);
  return false;
}

void DLib::Close() {
  if (handle_ == nullptr) return;
  uv_dlclose(&lib_);
  handle_ = nullptr;
}

void* DLib::GetSymbolAddress(const char* name) {
  void* address;
  if (0 == uv_dlsym(&lib_, name, &address)) return address;
  return nullptr;
}
#endif  // !__POSIX__

using InitializerCallback = void (*)(Local<Object> exports,
                                     Local<Value> module,
                                     Local<Context> context);

inline InitializerCallback GetInitializerCallback(DLib* dlib) {
  const char* name = "node_register_module_v" STRINGIFY(NODE_MODULE_VERSION);
  return reinterpret_cast<InitializerCallback>(dlib->GetSymbolAddress(name));
}

inline napi_addon_register_func GetNapiInitializerCallback(DLib* dlib) {
  const char* name =
      STRINGIFY(NAPI_MODULE_INITIALIZER_BASE) STRINGIFY(NAPI_MODULE_VERSION);
  return
      reinterpret_cast<napi_addon_register_func>(dlib->GetSymbolAddress(name));
}

void InitModpendingOnce() {
  CHECK_EQ(0, uv_key_create(&thread_local_modpending));
}

// DLOpen is process.dlopen(module, filename, flags).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
static void DLOpen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto context = env->context();

  uv_once(&init_modpending_once, InitModpendingOnce);
  CHECK_NULL(uv_key_get(&thread_local_modpending));

  if (args.Length() < 2) {
    env->ThrowError("process.dlopen needs at least 2 arguments.");
    return;
  }

  int32_t flags = DLib::kDefaultFlags;
  if (args.Length() > 2 && !args[2]->Int32Value(context).To(&flags)) {
    return env->ThrowTypeError("flag argument must be an integer.");
  }

  Local<Object> module;
  Local<Object> exports;
  Local<Value> exports_v;
  if (!args[0]->ToObject(context).ToLocal(&module) ||
      !module->Get(context, env->exports_string()).ToLocal(&exports_v) ||
      !exports_v->ToObject(context).ToLocal(&exports)) {
    return;  // Exception pending.
  }

  node::Utf8Value filename(env->isolate(), args[1]);  // Cast
  DLib dlib(*filename, flags);
  bool is_opened = dlib.Open();

  // Objects containing v14 or later modules will have registered themselves
  // on the pending list.  Activate all of them now.  At present, only one
  // module per object is supported.
  node_module* const mp = static_cast<node_module*>(
      uv_key_get(&thread_local_modpending));
  uv_key_set(&thread_local_modpending, nullptr);

  if (!is_opened) {
    Local<String> errmsg = OneByteString(env->isolate(), dlib.errmsg_.c_str());
    dlib.Close();
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(
        env->isolate(), errmsg, args[1]->ToString(context).ToLocalChecked());
#endif  // _WIN32
    env->isolate()->ThrowException(Exception::Error(errmsg));
    return;
  }

  if (mp == nullptr) {
    if (auto callback = GetInitializerCallback(&dlib)) {
      callback(exports, module, context);
    } else if (auto napi_callback = GetNapiInitializerCallback(&dlib)) {
      napi_module_register_by_symbol(exports, module, context, napi_callback);
    } else {
      dlib.Close();
      env->ThrowError("Module did not self-register.");
    }
    return;
  }

  // -1 is used for N-API modules
  if ((mp->nm_version != -1) && (mp->nm_version != NODE_MODULE_VERSION)) {
    // Even if the module did self-register, it may have done so with the wrong
    // version. We must only give up after having checked to see if it has an
    // appropriate initializer callback.
    if (auto callback = GetInitializerCallback(&dlib)) {
      callback(exports, module, context);
      return;
    }
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "The module '%s'"
             "\nwas compiled against a different Node.js version using"
             "\nNODE_MODULE_VERSION %d. This version of Node.js requires"
             "\nNODE_MODULE_VERSION %d. Please try re-compiling or "
             "re-installing\nthe module (for instance, using `npm rebuild` "
             "or `npm install`).",
             *filename, mp->nm_version, NODE_MODULE_VERSION);

    // NOTE: `mp` is allocated inside of the shared library's memory, calling
    // `dlclose` will deallocate it
    dlib.Close();
    env->ThrowError(errmsg);
    return;
  }
  if (mp->nm_flags & NM_F_BUILTIN) {
    dlib.Close();
    env->ThrowError("Built-in module self-registered.");
    return;
  }

  mp->nm_dso_handle = dlib.handle_;
  mp->nm_link = modlist_addon;
  modlist_addon = mp;

  if (mp->nm_context_register_func != nullptr) {
    mp->nm_context_register_func(exports, module, context, mp->nm_priv);
  } else if (mp->nm_register_func != nullptr) {
    mp->nm_register_func(exports, module, mp->nm_priv);
  } else {
    dlib.Close();
    env->ThrowError("Module has no declared entry point.");
    return;
  }

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
}


static void OnFatalError(const char* location, const char* message) {
  if (location) {
    PrintErrorString("FATAL ERROR: %s %s\n", location, message);
  } else {
    PrintErrorString("FATAL ERROR: %s\n", message);
  }
  fflush(stderr);
  ABORT();
}


[[noreturn]] void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to suppress compiler warning
  ABORT();
}


FatalTryCatch::~FatalTryCatch() {
  if (HasCaught()) {
    HandleScope scope(env_->isolate());
    ReportException(env_, *this);
    exit(7);
  }
}


void FatalException(Isolate* isolate,
                    Local<Value> error,
                    Local<Message> message,
                    bool from_promise) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  CHECK_NOT_NULL(env);  // TODO(addaleax): Handle nullptr here.
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Value> fatal_exception_function =
      process_object->Get(fatal_exception_string);

  if (!fatal_exception_function->IsFunction()) {
    // Failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(env, error, message);
    exit(6);
  } else {
    TryCatch fatal_try_catch(isolate);

    // Do not call FatalException when _fatalException handler throws
    fatal_try_catch.SetVerbose(false);

    Local<Value> argv[2] = { error,
                             Boolean::New(env->isolate(), from_promise) };

    // This will return true if the JS layer handled it, false otherwise
    MaybeLocal<Value> caught = fatal_exception_function.As<Function>()->Call(
        env->context(), process_object, arraysize(argv), argv);

    if (fatal_try_catch.HasTerminated())
      return;

    if (fatal_try_catch.HasCaught()) {
      // The fatal exception function threw, so we must exit
      ReportException(env, fatal_try_catch);
      exit(7);
    } else if (caught.ToLocalChecked()->IsFalse()) {
      ReportException(env, error, message);

      // fatal_exception_function call before may have set a new exit code ->
      // read it again, otherwise use default for uncaughtException 1
      Local<String> exit_code = env->exit_code_string();
      Local<Value> code;
      if (!process_object->Get(env->context(), exit_code).ToLocal(&code) ||
          !code->IsInt32()) {
        exit(1);
      }
      exit(code.As<Int32>()->Value());
    }
  }
}

void FatalException(Isolate* isolate,
                    Local<Value> error,
                    Local<Message> message) {
  FatalException(isolate, error, message, false /* from_promise */);
}

void FatalException(Isolate* isolate, const TryCatch& try_catch) {
  // If we try to print out a termination exception, we'd just get 'null',
  // so just crashing here with that information seems like a better idea,
  // and in particular it seems like we should handle terminations at the call
  // site for this function rather than by printing them out somewhere.
  CHECK(!try_catch.HasTerminated());

  HandleScope scope(isolate);
  if (!try_catch.IsVerbose()) {
    FatalException(isolate, try_catch.Exception(), try_catch.Message());
  }
}


static void OnMessage(Local<Message> message, Local<Value> error) {
  // The current version of V8 sends messages for errors only
  // (thus `error` is always set).
  FatalException(Isolate::GetCurrent(), error, message);
}

static Maybe<bool> ProcessEmitWarningGeneric(Environment* env,
                                             const char* warning,
                                             const char* type = nullptr,
                                             const char* code = nullptr) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> process = env->process_object();
  Local<Value> emit_warning;
  if (!process->Get(env->context(),
                    env->emit_warning_string()).ToLocal(&emit_warning)) {
    return Nothing<bool>();
  }

  if (!emit_warning->IsFunction()) return Just(false);

  int argc = 0;
  Local<Value> args[3];  // warning, type, code

  // The caller has to be able to handle a failure anyway, so we might as well
  // do proper error checking for string creation.
  if (!String::NewFromUtf8(env->isolate(),
                           warning,
                           NewStringType::kNormal).ToLocal(&args[argc++])) {
    return Nothing<bool>();
  }
  if (type != nullptr) {
    if (!String::NewFromOneByte(env->isolate(),
                                reinterpret_cast<const uint8_t*>(type),
                                NewStringType::kNormal)
                                    .ToLocal(&args[argc++])) {
      return Nothing<bool>();
    }
    if (code != nullptr &&
        !String::NewFromOneByte(env->isolate(),
                                reinterpret_cast<const uint8_t*>(code),
                                NewStringType::kNormal)
                                    .ToLocal(&args[argc++])) {
      return Nothing<bool>();
    }
  }

  // MakeCallback() unneeded because emitWarning is internal code, it calls
  // process.emit('warning', ...), but does so on the nextTick.
  if (emit_warning.As<Function>()->Call(env->context(),
                                        process,
                                        argc,
                                        args).IsEmpty()) {
    return Nothing<bool>();
  }
  return Just(true);
}


// Call process.emitWarning(str), fmt is a snprintf() format string
Maybe<bool> ProcessEmitWarning(Environment* env, const char* fmt, ...) {
  char warning[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(warning, sizeof(warning), fmt, ap);
  va_end(ap);

  return ProcessEmitWarningGeneric(env, warning);
}


Maybe<bool> ProcessEmitDeprecationWarning(Environment* env,
                                          const char* warning,
                                          const char* deprecation_code) {
  return ProcessEmitWarningGeneric(env,
                                   warning,
                                   "DeprecationWarning",
                                   deprecation_code);
}


static Local<Object> InitModule(Environment* env,
                                 node_module* mod,
                                 Local<String> module) {
  Local<Object> exports = Object::New(env->isolate());
  // Internal bindings don't have a "module" object, only exports.
  CHECK_NULL(mod->nm_register_func);
  CHECK_NOT_NULL(mod->nm_context_register_func);
  Local<Value> unused = Undefined(env->isolate());
  mod->nm_context_register_func(exports,
                                unused,
                                env->context(),
                                mod->nm_priv);
  return exports;
}

static void ThrowIfNoSuchModule(Environment* env, const char* module_v) {
  char errmsg[1024];
  snprintf(errmsg,
           sizeof(errmsg),
           "No such module: %s",
           module_v);
  env->ThrowError(errmsg);
}

static void GetBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());

  Local<String> module = args[0].As<String>();
  node::Utf8Value module_v(env->isolate(), module);

  node_module* mod = get_builtin_module(*module_v);
  Local<Object> exports;
  if (mod != nullptr) {
    exports = InitModule(env, mod, module);
  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New(env->isolate());
    CHECK(exports->SetPrototype(env->context(),
                                Null(env->isolate())).FromJust());
    DefineConstants(env->isolate(), exports);
  } else if (!strcmp(*module_v, "natives")) {
    exports = Object::New(env->isolate());
    DefineJavaScript(env, exports);
  } else {
    return ThrowIfNoSuchModule(env, *module_v);
  }

  args.GetReturnValue().Set(exports);
}

static void GetInternalBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());

  Local<String> module = args[0].As<String>();
  node::Utf8Value module_v(env->isolate(), module);
  Local<Object> exports;

  node_module* mod = get_internal_module(*module_v);
  if (mod != nullptr) {
    exports = InitModule(env, mod, module);
  } else if (!strcmp(*module_v, "code_cache")) {
    // internalBinding('code_cache')
    exports = Object::New(env->isolate());
    DefineCodeCache(env, exports);
  } else if (!strcmp(*module_v, "code_cache_hash")) {
    // internalBinding('code_cache_hash')
    exports = Object::New(env->isolate());
    DefineCodeCacheHash(env, exports);
  } else if (!strcmp(*module_v, "natives_hash")) {
    // internalBinding('natives_hash')
    exports = Object::New(env->isolate());
    DefineJavaScriptHash(env, exports);
  } else {
    return ThrowIfNoSuchModule(env, *module_v);
  }

  args.GetReturnValue().Set(exports);
}

static void GetLinkedBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());

  Local<String> module_name = args[0].As<String>();

  node::Utf8Value module_name_v(env->isolate(), module_name);
  node_module* mod = get_linked_module(*module_name_v);

  if (mod == nullptr) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "No such module was linked: %s",
             *module_name_v);
    return env->ThrowError(errmsg);
  }

  Local<Object> module = Object::New(env->isolate());
  Local<Object> exports = Object::New(env->isolate());
  Local<String> exports_prop = String::NewFromUtf8(env->isolate(), "exports",
      NewStringType::kNormal).ToLocalChecked();
  module->Set(exports_prop, exports);

  if (mod->nm_context_register_func != nullptr) {
    mod->nm_context_register_func(exports,
                                  module,
                                  env->context(),
                                  mod->nm_priv);
  } else if (mod->nm_register_func != nullptr) {
    mod->nm_register_func(exports, module, mod->nm_priv);
  } else {
    return env->ThrowError("Linked module has no declared entry point.");
  }

  auto effective_exports = module->Get(exports_prop);

  args.GetReturnValue().Set(effective_exports);
}

static Local<Object> GetFeatures(Environment* env) {
  EscapableHandleScope scope(env->isolate());

  Local<Object> obj = Object::New(env->isolate());
#if defined(DEBUG) && DEBUG
  Local<Value> debug = True(env->isolate());
#else
  Local<Value> debug = False(env->isolate());
#endif  // defined(DEBUG) && DEBUG

  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "debug"), debug);
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "uv"), True(env->isolate()));
  // TODO(bnoordhuis) ping libuv
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "ipv6"), True(env->isolate()));

#ifdef HAVE_OPENSSL
  Local<Boolean> have_openssl = True(env->isolate());
#else
  Local<Boolean> have_openssl = False(env->isolate());
#endif

  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_alpn"), have_openssl);
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_sni"), have_openssl);
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_ocsp"), have_openssl);
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls"), have_openssl);

  return scope.Escape(obj);
}

static void DebugProcess(const FunctionCallbackInfo<Value>& args);
static void DebugEnd(const FunctionCallbackInfo<Value>& args);

namespace {

#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           ReadOnly).FromJust();                              \
  } while (0)

#define READONLY_DONT_ENUM_PROPERTY(obj, str, var)                            \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           static_cast<PropertyAttribute>(ReadOnly|DontEnum)) \
        .FromJust();                                                          \
  } while (0)

}  // anonymous namespace

void SetupProcessObject(Environment* env,
                        const std::vector<std::string>& args,
                        const std::vector<std::string>& exec_args) {
  HandleScope scope(env->isolate());

  Local<Object> process = env->process_object();

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

  const char http_parser_version[] = NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
                                     "."
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)
                                     "."
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);

  const std::string brotli_version =
      std::to_string(BrotliEncoderVersion() >> 24) +
      "." +
      std::to_string((BrotliEncoderVersion() & 0xFFF000) >> 12) +
      "." +
      std::to_string(BrotliEncoderVersion() & 0xFFF);

  READONLY_PROPERTY(versions,
                    "http_parser",
                    FIXED_ONE_BYTE_STRING(env->isolate(), http_parser_version));
  // +1 to get rid of the leading 'v'
  READONLY_PROPERTY(versions,
                    "node",
                    OneByteString(env->isolate(), NODE_VERSION + 1));
  READONLY_PROPERTY(versions,
                    "v8",
                    OneByteString(env->isolate(), V8::GetVersion()));
  READONLY_PROPERTY(versions,
                    "uv",
                    OneByteString(env->isolate(), uv_version_string()));
  READONLY_PROPERTY(versions,
                    "zlib",
                    FIXED_ONE_BYTE_STRING(env->isolate(), ZLIB_VERSION));
  READONLY_PROPERTY(versions,
                    "brotli",
                    OneByteString(env->isolate(), brotli_version.c_str()));
  READONLY_PROPERTY(versions,
                    "ares",
                    FIXED_ONE_BYTE_STRING(env->isolate(), ARES_VERSION_STR));

  const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);
  READONLY_PROPERTY(
      versions,
      "modules",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_modules_version));
  READONLY_PROPERTY(versions,
                    "nghttp2",
                    FIXED_ONE_BYTE_STRING(env->isolate(), NGHTTP2_VERSION));
  const char node_napi_version[] = NODE_STRINGIFY(NAPI_VERSION);
  READONLY_PROPERTY(
      versions,
      "napi",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_napi_version));

#if HAVE_OPENSSL
  READONLY_PROPERTY(
      versions,
      "openssl",
      OneByteString(env->isolate(), crypto::GetOpenSSLVersion().c_str()));
#endif

  // process.arch
  READONLY_PROPERTY(process, "arch", OneByteString(env->isolate(), NODE_ARCH));

  // process.platform
  READONLY_PROPERTY(process,
                    "platform",
                    OneByteString(env->isolate(), NODE_PLATFORM));

  // process.release
  Local<Object> release = Object::New(env->isolate());
  READONLY_PROPERTY(process, "release", release);
  READONLY_PROPERTY(release, "name",
                    OneByteString(env->isolate(), NODE_RELEASE));

#if NODE_VERSION_IS_LTS
  READONLY_PROPERTY(release, "lts",
                    OneByteString(env->isolate(), NODE_VERSION_LTS_CODENAME));
#endif

// if this is a release build and no explicit base has been set
// substitute the standard release download URL
#ifndef NODE_RELEASE_URLBASE
# if NODE_VERSION_IS_RELEASE
#  define NODE_RELEASE_URLBASE "https://nodejs.org/download/release/"
# endif
#endif

#if defined(NODE_RELEASE_URLBASE)
#  define NODE_RELEASE_URLPFX NODE_RELEASE_URLBASE "v" NODE_VERSION_STRING "/"
#  define NODE_RELEASE_URLFPFX NODE_RELEASE_URLPFX "node-v" NODE_VERSION_STRING

  READONLY_PROPERTY(release,
                    "sourceUrl",
                    OneByteString(env->isolate(),
                    NODE_RELEASE_URLFPFX ".tar.gz"));
  READONLY_PROPERTY(release,
                    "headersUrl",
                    OneByteString(env->isolate(),
                    NODE_RELEASE_URLFPFX "-headers.tar.gz"));
#  ifdef _WIN32
  READONLY_PROPERTY(release,
                    "libUrl",
                    OneByteString(env->isolate(),
                    strcmp(NODE_ARCH, "ia32") ? NODE_RELEASE_URLPFX "win-"
                                                NODE_ARCH "/node.lib"
                                              : NODE_RELEASE_URLPFX
                                                "win-x86/node.lib"));
#  endif
#endif

  // process.argv
  Local<Array> arguments = Array::New(env->isolate(), args.size());
  for (size_t i = 0; i < args.size(); ++i) {
    arguments->Set(env->context(), i,
        String::NewFromUtf8(env->isolate(), args[i].c_str(),
                            NewStringType::kNormal).ToLocalChecked())
        .FromJust();
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "argv"), arguments);

  // process.execArgv
  Local<Array> exec_arguments = Array::New(env->isolate(), exec_args.size());
  for (size_t i = 0; i < exec_args.size(); ++i) {
    exec_arguments->Set(env->context(), i,
        String::NewFromUtf8(env->isolate(), exec_args[i].c_str(),
                            NewStringType::kNormal).ToLocalChecked())
        .FromJust();
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "execArgv"),
               exec_arguments);

  // create process.env
  Local<ObjectTemplate> process_env_template =
      ObjectTemplate::New(env->isolate());
  process_env_template->SetHandler(NamedPropertyHandlerConfiguration(
          EnvGetter,
          EnvSetter,
          EnvQuery,
          EnvDeleter,
          EnvEnumerator,
          env->as_external()));

  Local<Object> process_env =
      process_env_template->NewInstance(env->context()).ToLocalChecked();
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "env"), process_env);

  READONLY_PROPERTY(process, "pid",
                    Integer::New(env->isolate(), uv_os_getpid()));
  READONLY_PROPERTY(process, "features", GetFeatures(env));

  CHECK(process->SetAccessor(env->context(),
                             FIXED_ONE_BYTE_STRING(env->isolate(), "ppid"),
                             GetParentProcessId).FromJust());

  // -e, --eval
  if (env->options()->has_eval_string) {
    READONLY_PROPERTY(process,
                      "_eval",
                      String::NewFromUtf8(
                          env->isolate(),
                          env->options()->eval_string.c_str(),
                          NewStringType::kNormal).ToLocalChecked());
  }

  // -p, --print
  if (env->options()->print_eval) {
    READONLY_PROPERTY(process, "_print_eval", True(env->isolate()));
  }

  // -c, --check
  if (env->options()->syntax_check_only) {
    READONLY_PROPERTY(process, "_syntax_check_only", True(env->isolate()));
  }

  // -i, --interactive
  if (env->options()->force_repl) {
    READONLY_PROPERTY(process, "_forceRepl", True(env->isolate()));
  }

  // -r, --require
  std::vector<std::string> preload_modules =
      std::move(env->options()->preload_modules);
  if (!preload_modules.empty()) {
    Local<Array> array = Array::New(env->isolate());
    for (unsigned int i = 0; i < preload_modules.size(); ++i) {
      Local<String> module = String::NewFromUtf8(env->isolate(),
                                                 preload_modules[i].c_str(),
                                                 NewStringType::kNormal)
                                 .ToLocalChecked();
      array->Set(i, module);
    }
    READONLY_PROPERTY(process,
                      "_preload_modules",
                      array);

    preload_modules.clear();
  }

  // --no-deprecation
  // TODO(addaleax): Uncomment the commented part.
  if (/*env->options()->*/no_deprecation) {
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
  if (env->options()->prof_process) {
    READONLY_PROPERTY(process, "profProcess", True(env->isolate()));
  }

  // --trace-deprecation
  if (env->options()->trace_deprecation) {
    READONLY_PROPERTY(process, "traceDeprecation", True(env->isolate()));
  }

  // TODO(refack): move the following 4 to `node_config`
  // --inspect-brk
  if (env->options()->debug_options->wait_for_connect()) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_breakFirstLine", True(env->isolate()));
  }

  if (env->options()->debug_options->break_node_first_line) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_breakNodeFirstLine", True(env->isolate()));
  }

  // --inspect --debug-brk
  if (env->options()->debug_options->deprecated_invocation()) {
    READONLY_DONT_ENUM_PROPERTY(process,
                                "_deprecatedDebugBrk", True(env->isolate()));
  }

  // --debug or, --debug-brk without --inspect
  if (env->options()->debug_options->invalid_invocation()) {
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

  size_t exec_path_len = 2 * PATH_MAX;
  char* exec_path = new char[exec_path_len];
  Local<String> exec_path_value;
  if (uv_exepath(exec_path, &exec_path_len) == 0) {
    exec_path_value = String::NewFromUtf8(env->isolate(),
                                          exec_path,
                                          NewStringType::kInternalized,
                                          exec_path_len).ToLocalChecked();
  } else {
    exec_path_value = String::NewFromUtf8(env->isolate(), args[0].c_str(),
        NewStringType::kInternalized).ToLocalChecked();
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "execPath"),
               exec_path_value);
  delete[] exec_path;

  auto debug_port_string = FIXED_ONE_BYTE_STRING(env->isolate(), "debugPort");
  CHECK(process->SetAccessor(env->context(),
                             debug_port_string,
                             DebugPortGetter,
                             env->is_main_thread() ? DebugPortSetter : nullptr,
                             env->as_external()).FromJust());

  // define various internal methods
  if (env->is_main_thread()) {
    env->SetMethod(process, "_debugProcess", DebugProcess);
    env->SetMethod(process, "_debugEnd", DebugEnd);
    env->SetMethod(process,
                   "_startProfilerIdleNotifier",
                   StartProfilerIdleNotifier);
    env->SetMethod(process,
                   "_stopProfilerIdleNotifier",
                   StopProfilerIdleNotifier);
    env->SetMethod(process, "abort", Abort);
    env->SetMethod(process, "chdir", Chdir);
    env->SetMethod(process, "umask", Umask);
  }
  env->SetMethod(process, "_getActiveRequests", GetActiveRequests);
  env->SetMethod(process, "_getActiveHandles", GetActiveHandles);
  env->SetMethod(process, "_kill", Kill);

  env->SetMethodNoSideEffect(process, "cwd", Cwd);
  env->SetMethod(process, "dlopen", DLOpen);
  env->SetMethod(process, "reallyExit", Exit);
  env->SetMethodNoSideEffect(process, "uptime", Uptime);

#if defined(__POSIX__) && !defined(__ANDROID__) && !defined(__CloudABI__)
  env->SetMethodNoSideEffect(process, "getuid", GetUid);
  env->SetMethodNoSideEffect(process, "geteuid", GetEUid);
  env->SetMethodNoSideEffect(process, "getgid", GetGid);
  env->SetMethodNoSideEffect(process, "getegid", GetEGid);
  env->SetMethodNoSideEffect(process, "getgroups", GetGroups);
#endif  // __POSIX__ && !defined(__ANDROID__) && !defined(__CloudABI__)
}


#undef READONLY_PROPERTY


void SignalExit(int signo) {
  uv_tty_reset_mode();
#ifdef __FreeBSD__
  // FreeBSD has a nasty bug, see RegisterSignalHandler for details
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(signo, &sa, nullptr), 0);
#endif
  raise(signo);
}


static MaybeLocal<Function> GetBootstrapper(
    Environment* env,
    Local<String> source,
    Local<String> script_name) {
  EscapableHandleScope scope(env->isolate());

  TryCatch try_catch(env->isolate());

  // Disable verbose mode to stop FatalException() handler from trying
  // to handle the exception. Errors this early in the start-up phase
  // are not safe to ignore.
  try_catch.SetVerbose(false);

  // Execute the bootstrapper javascript file
  MaybeLocal<Value> bootstrapper_v = ExecuteString(env, source, script_name);
  if (bootstrapper_v.IsEmpty())  // This happens when execution was interrupted.
    return MaybeLocal<Function>();

  if (try_catch.HasCaught())  {
    ReportException(env, try_catch);
    exit(10);
  }

  CHECK(bootstrapper_v.ToLocalChecked()->IsFunction());
  return scope.Escape(bootstrapper_v.ToLocalChecked().As<Function>());
}

static bool ExecuteBootstrapper(Environment* env, Local<Function> bootstrapper,
                                int argc, Local<Value> argv[],
                                Local<Value>* out) {
  bool ret = bootstrapper->Call(
      env->context(), Null(env->isolate()), argc, argv).ToLocal(out);

  // If there was an error during bootstrap then it was either handled by the
  // FatalException handler or it's unrecoverable (e.g. max call stack
  // exceeded). Either way, clear the stack so that the AsyncCallbackScope
  // destructor doesn't fail on the id check.
  // There are only two ways to have a stack size > 1: 1) the user manually
  // called MakeCallback or 2) user awaited during bootstrap, which triggered
  // _tickCallback().
  if (!ret) {
    env->async_hooks()->clear_async_id_stack();
  }

  return ret;
}


void LoadEnvironment(Environment* env) {
  HandleScope handle_scope(env->isolate());

  TryCatch try_catch(env->isolate());
  // Disable verbose mode to stop FatalException() handler from trying
  // to handle the exception. Errors this early in the start-up phase
  // are not safe to ignore.
  try_catch.SetVerbose(false);

  // The bootstrapper scripts are lib/internal/bootstrap/loaders.js and
  // lib/internal/bootstrap/node.js, each included as a static C string
  // defined in node_javascript.h, generated in node_javascript.cc by
  // node_js2c.
  Local<String> loaders_name =
      FIXED_ONE_BYTE_STRING(env->isolate(), "internal/bootstrap/loaders.js");
  MaybeLocal<Function> loaders_bootstrapper =
      GetBootstrapper(env, LoadersBootstrapperSource(env), loaders_name);
  Local<String> node_name =
      FIXED_ONE_BYTE_STRING(env->isolate(), "internal/bootstrap/node.js");
  MaybeLocal<Function> node_bootstrapper =
      GetBootstrapper(env, NodeBootstrapperSource(env), node_name);

  if (loaders_bootstrapper.IsEmpty() || node_bootstrapper.IsEmpty()) {
    // Execution was interrupted.
    return;
  }

  // Add a reference to the global object
  Local<Object> global = env->context()->Global();

#if defined HAVE_DTRACE || defined HAVE_ETW
  InitDTrace(env, global);
#endif

#if defined HAVE_PERFCTR
  InitPerfCounters(env, global);
#endif

  // Enable handling of uncaught exceptions
  // (FatalException(), break on uncaught exception in debugger)
  //
  // This is not strictly necessary since it's almost impossible
  // to attach the debugger fast enough to break on exception
  // thrown during process startup.
  try_catch.SetVerbose(true);

  env->SetMethod(env->process_object(), "_rawDebug", RawDebug);

  // Expose the global object as a property on itself
  // (Allows you to set stuff on `global` from anywhere in JavaScript.)
  global->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "global"), global);

  // Create binding loaders
  Local<Function> get_binding_fn =
      env->NewFunctionTemplate(GetBinding)->GetFunction(env->context())
          .ToLocalChecked();

  Local<Function> get_linked_binding_fn =
      env->NewFunctionTemplate(GetLinkedBinding)->GetFunction(env->context())
          .ToLocalChecked();

  Local<Function> get_internal_binding_fn =
      env->NewFunctionTemplate(GetInternalBinding)->GetFunction(env->context())
          .ToLocalChecked();

  Local<Value> loaders_bootstrapper_args[] = {
    env->process_object(),
    get_binding_fn,
    get_linked_binding_fn,
    get_internal_binding_fn,
    Boolean::New(env->isolate(),
                 env->options()->debug_options->break_node_first_line)
  };

  // Bootstrap internal loaders
  Local<Value> bootstrapped_loaders;
  if (!ExecuteBootstrapper(env, loaders_bootstrapper.ToLocalChecked(),
                           arraysize(loaders_bootstrapper_args),
                           loaders_bootstrapper_args,
                           &bootstrapped_loaders)) {
    return;
  }

  // Bootstrap Node.js
  Local<Object> bootstrapper = Object::New(env->isolate());
  SetupBootstrapObject(env, bootstrapper);
  Local<Value> bootstrapped_node;
  Local<Value> node_bootstrapper_args[] = {
    env->process_object(),
    bootstrapper,
    bootstrapped_loaders
  };
  if (!ExecuteBootstrapper(env, node_bootstrapper.ToLocalChecked(),
                           arraysize(node_bootstrapper_args),
                           node_bootstrapper_args,
                           &bootstrapped_node)) {
    return;
  }
}


static void StartInspector(Environment* env, const char* path,
                           std::shared_ptr<DebugOptions> debug_options) {
#if HAVE_INSPECTOR
  CHECK(!env->inspector_agent()->IsListening());
  v8_platform.StartInspector(env, path, debug_options);
#endif  // HAVE_INSPECTOR
}


#ifdef __POSIX__
void RegisterSignalHandler(int signal,
                           void (*handler)(int signal),
                           bool reset_handler) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
#ifndef __FreeBSD__
  // FreeBSD has a nasty bug with SA_RESETHAND reseting the SA_SIGINFO, that is
  // in turn set for a libthr wrapper. This leads to a crash.
  // Work around the issue by manually setting SIG_DFL in the signal handler
  sa.sa_flags = reset_handler ? SA_RESETHAND : 0;
#endif
  sigfillset(&sa.sa_mask);
  CHECK_EQ(sigaction(signal, &sa, nullptr), 0);
}


void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 1) {
    return env->ThrowError("Invalid number of arguments.");
  }

  CHECK(args[0]->IsNumber());
  pid_t pid = args[0].As<Integer>()->Value();
  int r = kill(pid, SIGUSR1);

  if (r != 0) {
    return env->ThrowErrnoException(errno, "kill");
  }
}
#endif  // __POSIX__


#ifdef _WIN32
static int GetDebugSignalHandlerMappingName(DWORD pid, wchar_t* buf,
    size_t buf_len) {
  return _snwprintf(buf, buf_len, L"node-debug-handler-%u", pid);
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    env->ThrowError("Invalid number of arguments.");
    return;
  }

  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE mapping = nullptr;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = nullptr;
  DWORD pid = 0;

  OnScopeLeave cleanup([&]() {
    if (process != nullptr)
      CloseHandle(process);
    if (thread != nullptr)
      CloseHandle(thread);
    if (handler != nullptr)
      UnmapViewOfFile(handler);
    if (mapping != nullptr)
      CloseHandle(mapping);
  });

  CHECK(args[0]->IsNumber());
  pid = args[0].As<Integer>()->Value();

  process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                            PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                            PROCESS_VM_READ,
                        FALSE,
                        pid);
  if (process == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "OpenProcess"));
    return;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    env->ThrowErrnoException(errno, "sprintf");
    return;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                             GetLastError(),
                                             "OpenFileMappingW"));
    return;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping,
                    FILE_MAP_READ,
                    0,
                    0,
                    sizeof *handler));
  if (handler == nullptr || *handler == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "MapViewOfFile"));
    return;
  }

  thread = CreateRemoteThread(process,
                              nullptr,
                              0,
                              *handler,
                              nullptr,
                              0,
                              nullptr);
  if (thread == nullptr) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                                 GetLastError(),
                                                 "CreateRemoteThread"));
    return;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                                 GetLastError(),
                                                 "WaitForSingleObject"));
    return;
  }
}
#endif  // _WIN32


static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
#if HAVE_INSPECTOR
  Environment* env = Environment::GetCurrent(args);
  if (env->inspector_agent()->IsListening()) {
    env->inspector_agent()->Stop();
  }
#endif
}


inline void PlatformInit() {
#ifdef __POSIX__
#if HAVE_INSPECTOR
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  const int err = pthread_sigmask(SIG_SETMASK, &sigmask, nullptr);
#endif  // HAVE_INSPECTOR

  // Make sure file descriptors 0-2 are valid before we start logging anything.
  for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd += 1) {
    struct stat ignored;
    if (fstat(fd, &ignored) == 0)
      continue;
    // Anything but EBADF means something is seriously wrong.  We don't
    // have to special-case EINTR, fstat() is not interruptible.
    if (errno != EBADF)
      ABORT();
    if (fd != open("/dev/null", O_RDWR))
      ABORT();
  }

#if HAVE_INSPECTOR
  CHECK_EQ(err, 0);
#endif  // HAVE_INSPECTOR

#ifndef NODE_SHARED_MODE
  // Restore signal dispositions, the parent process may have changed them.
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  // The hard-coded upper limit is because NSIG is not very reliable; on Linux,
  // it evaluates to 32, 34 or 64, depending on whether RT signals are enabled.
  // Counting up to SIGRTMIN doesn't work for the same reason.
  for (unsigned nr = 1; nr < kMaxSignal; nr += 1) {
    if (nr == SIGKILL || nr == SIGSTOP)
      continue;
    act.sa_handler = (nr == SIGPIPE) ? SIG_IGN : SIG_DFL;
    CHECK_EQ(0, sigaction(nr, &act, nullptr));
  }
#endif  // !NODE_SHARED_MODE

  RegisterSignalHandler(SIGINT, SignalExit, true);
  RegisterSignalHandler(SIGTERM, SignalExit, true);

  // Raise the open file descriptor limit.
  struct rlimit lim;
  if (getrlimit(RLIMIT_NOFILE, &lim) == 0 && lim.rlim_cur != lim.rlim_max) {
    // Do a binary search for the limit.
    rlim_t min = lim.rlim_cur;
    rlim_t max = 1 << 20;
    // But if there's a defined upper bound, don't search, just set it.
    if (lim.rlim_max != RLIM_INFINITY) {
      min = lim.rlim_max;
      max = lim.rlim_max;
    }
    do {
      lim.rlim_cur = min + (max - min) / 2;
      if (setrlimit(RLIMIT_NOFILE, &lim)) {
        max = lim.rlim_cur;
      } else {
        min = lim.rlim_cur;
      }
    } while (min + 1 < max);
  }
#endif  // __POSIX__
#ifdef _WIN32
  for (int fd = 0; fd <= 2; ++fd) {
    auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE ||
        GetFileType(handle) == FILE_TYPE_UNKNOWN) {
      // Ignore _close result. If it fails or not depends on used Windows
      // version. We will just check _open result.
      _close(fd);
      if (fd != _open("nul", _O_RDWR))
        ABORT();
    }
  }
#endif  // _WIN32
}

// TODO(addaleax): Remove, both from the public API and in implementation.
bool no_deprecation = false;
#if HAVE_OPENSSL
bool ssl_openssl_cert_store = false;
#if NODE_FIPS_MODE
bool enable_fips_crypto = false;
bool force_fips_crypto = false;
#endif
#endif

void ProcessArgv(std::vector<std::string>* args,
                 std::vector<std::string>* exec_args,
                 bool is_env) {
  // Parse a few arguments which are specific to Node.
  std::vector<std::string> v8_args;
  std::vector<std::string> errors{};

  {
    // TODO(addaleax): The mutex here should ideally be held during the
    // entire function, but that doesn't play well with the exit() calls below.
    Mutex::ScopedLock lock(per_process_opts_mutex);
    options_parser::PerProcessOptionsParser::instance.Parse(
        args,
        exec_args,
        &v8_args,
        per_process_opts.get(),
        is_env ? kAllowedInEnvironment : kDisallowedInEnvironment,
        &errors);
  }

  if (!errors.empty()) {
    for (auto const& error : errors) {
      fprintf(stderr, "%s: %s\n", args->at(0).c_str(), error.c_str());
    }
    exit(9);
  }

  if (per_process_opts->print_version) {
    printf("%s\n", NODE_VERSION);
    exit(0);
  }

  if (per_process_opts->print_v8_help) {
    V8::SetFlagsFromString("--help", 6);
    exit(0);
  }

  for (const std::string& cve : per_process_opts->security_reverts)
    Revert(cve.c_str());

  auto env_opts = per_process_opts->per_isolate->per_env;
  if (std::find(v8_args.begin(), v8_args.end(),
                "--abort-on-uncaught-exception") != v8_args.end() ||
      std::find(v8_args.begin(), v8_args.end(),
                "--abort_on_uncaught_exception") != v8_args.end()) {
    env_opts->abort_on_uncaught_exception = true;
  }

  // TODO(bnoordhuis) Intercept --prof arguments and start the CPU profiler
  // manually?  That would give us a little more control over its runtime
  // behavior but it could also interfere with the user's intentions in ways
  // we fail to anticipate.  Dillema.
  if (std::find(v8_args.begin(), v8_args.end(), "--prof") != v8_args.end()) {
    v8_is_profiling = true;
  }

#ifdef __POSIX__
  // Block SIGPROF signals when sleeping in epoll_wait/kevent/etc.  Avoids the
  // performance penalty of frequent EINTR wakeups when the profiler is running.
  // Only do this for v8.log profiling, as it breaks v8::CpuProfiler users.
  if (v8_is_profiling) {
    uv_loop_configure(uv_default_loop(), UV_LOOP_BLOCK_SIGNAL, SIGPROF);
  }
#endif

  std::vector<char*> v8_args_as_char_ptr(v8_args.size());
  if (v8_args.size() > 0) {
    for (size_t i = 0; i < v8_args.size(); ++i)
      v8_args_as_char_ptr[i] = &v8_args[i][0];
    int argc = v8_args.size();
    V8::SetFlagsFromCommandLine(&argc, &v8_args_as_char_ptr[0], true);
    v8_args_as_char_ptr.resize(argc);
  }

  // Anything that's still in v8_argv is not a V8 or a node option.
  for (size_t i = 1; i < v8_args_as_char_ptr.size(); i++) {
    fprintf(stderr, "%s: bad option: %s\n",
            args->at(0).c_str(), v8_args_as_char_ptr[i]);
  }

  if (v8_args_as_char_ptr.size() > 1) {
    exit(9);
  }

  // TODO(addaleax): Remove.
  zero_fill_all_buffers = per_process_opts->zero_fill_all_buffers;
  no_deprecation = per_process_opts->per_isolate->per_env->no_deprecation;
#if HAVE_OPENSSL
  ssl_openssl_cert_store = per_process_opts->ssl_openssl_cert_store;
#if NODE_FIPS_MODE
  enable_fips_crypto = per_process_opts->enable_fips_crypto;
  force_fips_crypto = per_process_opts->force_fips_crypto;
#endif
#endif
}


void Init(std::vector<std::string>* argv,
          std::vector<std::string>* exec_argv) {
  // Initialize prog_start_time to get relative uptime.
  prog_start_time = static_cast<double>(uv_now(uv_default_loop()));

  // Register built-in modules
  RegisterBuiltinModules();

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

  std::shared_ptr<EnvironmentOptions> default_env_options =
      per_process_opts->per_isolate->per_env;
  {
    std::string text;
    default_env_options->pending_deprecation =
        SafeGetenv("NODE_PENDING_DEPRECATION", &text) && text[0] == '1';
  }

  // Allow for environment set preserving symlinks.
  {
    std::string text;
    default_env_options->preserve_symlinks =
        SafeGetenv("NODE_PRESERVE_SYMLINKS", &text) && text[0] == '1';
  }

  {
    std::string text;
    default_env_options->preserve_symlinks_main =
        SafeGetenv("NODE_PRESERVE_SYMLINKS_MAIN", &text) && text[0] == '1';
  }

  if (default_env_options->redirect_warnings.empty()) {
    SafeGetenv("NODE_REDIRECT_WARNINGS",
               &default_env_options->redirect_warnings);
  }

#if HAVE_OPENSSL
  std::string* openssl_config = &per_process_opts->openssl_config;
  if (openssl_config->empty()) {
    SafeGetenv("OPENSSL_CONF", openssl_config);
  }
#endif

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  std::string node_options;

  if (SafeGetenv("NODE_OPTIONS", &node_options)) {
    std::vector<std::string> env_argv;
    // [0] is expected to be the program name, fill it in from the real argv.
    env_argv.push_back(argv->at(0));

    bool is_in_string = false;
    bool will_start_new_arg = true;
    for (std::string::size_type index = 0;
         index < node_options.size();
         ++index) {
      char c = node_options.at(index);

      // Backslashes escape the following character
      if (c == '\\' && is_in_string) {
        if (index + 1 == node_options.size()) {
          fprintf(stderr, "invalid value for NODE_OPTIONS "
                          "(invalid escape)\n");
          exit(9);
        } else {
          c = node_options.at(++index);
        }
      } else if (c == ' ' && !is_in_string) {
        will_start_new_arg = true;
        continue;
      } else if (c == '"') {
        is_in_string = !is_in_string;
        continue;
      }

      if (will_start_new_arg) {
        env_argv.push_back(std::string(1, c));
        will_start_new_arg = false;
      } else {
        env_argv.back() += c;
      }
    }

    if (is_in_string) {
      fprintf(stderr, "invalid value for NODE_OPTIONS "
                      "(unterminated string)\n");
      exit(9);
    }


    ProcessArgv(&env_argv, nullptr, true);
  }
#endif

  ProcessArgv(argv, exec_argv, false);

  // Set the process.title immediately after processing argv if --title is set.
  if (!per_process_opts->title.empty())
    uv_set_process_title(per_process_opts->title.c_str());

#if defined(NODE_HAVE_I18N_SUPPORT)
  // If the parameter isn't given, use the env variable.
  if (per_process_opts->icu_data_dir.empty())
    SafeGetenv("NODE_ICU_DATA", &per_process_opts->icu_data_dir);
  // Initialize ICU.
  // If icu_data_dir is empty here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(per_process_opts->icu_data_dir)) {
    fprintf(stderr,
            "%s: could not initialize ICU "
            "(check NODE_ICU_DATA or --icu-data-dir parameters)\n",
            argv->at(0).c_str());
    exit(9);
  }
#endif

  // We should set node_is_initialized here instead of in node::Start,
  // otherwise embedders using node::Init to initialize everything will not be
  // able to set it and native modules will not load for them.
  node_is_initialized = true;
}

// TODO(addaleax): Deprecate and eventually remove this.
void Init(int* argc,
          const char** argv,
          int* exec_argc,
          const char*** exec_argv) {
  std::vector<std::string> argv_(argv, argv + *argc);  // NOLINT
  std::vector<std::string> exec_argv_;

  Init(&argv_, &exec_argv_);

  *argc = argv_.size();
  *exec_argc = exec_argv_.size();
  // These leak memory, because, in the original code of this function, no
  // extra allocations were visible. This should be okay because this function
  // is only supposed to be called once per process, though.
  *exec_argv = Malloc<const char*>(*exec_argc);
  for (int i = 0; i < *exec_argc; ++i)
    (*exec_argv)[i] = strdup(exec_argv_[i].c_str());
  for (int i = 0; i < *argc; ++i)
    argv[i] = strdup(argv_[i].c_str());
}

void RunAtExit(Environment* env) {
  env->RunAtExitCallbacks();
}


uv_loop_t* GetCurrentEventLoop(Isolate* isolate) {
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty())
    return nullptr;
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr)
    return nullptr;
  return env->event_loop();
}


void AtExit(void (*cb)(void* arg), void* arg) {
  auto env = Environment::GetThreadLocalEnv();
  AtExit(env, cb, arg);
}


void AtExit(Environment* env, void (*cb)(void* arg), void* arg) {
  CHECK_NOT_NULL(env);
  env->AtExit(cb, arg);
}


void RunBeforeExit(Environment* env) {
  env->RunBeforeExitCallbacks();

  if (!uv_loop_alive(env->event_loop()))
    EmitBeforeExit(env);
}


void EmitBeforeExit(Environment* env) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  Local<String> exit_code = env->exit_code_string();
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "beforeExit"),
    process_object->Get(env->context(), exit_code).ToLocalChecked()
        ->ToInteger(env->context()).ToLocalChecked()
  };
  MakeCallback(env->isolate(),
               process_object, "emit", arraysize(args), args,
               {0, 0}).ToLocalChecked();
}


int EmitExit(Environment* env) {
  // process.emit('exit')
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  process_object->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "_exiting"),
                      True(env->isolate()));

  Local<String> exit_code = env->exit_code_string();
  int code = process_object->Get(env->context(), exit_code).ToLocalChecked()
      ->Int32Value(env->context()).ToChecked();

  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "exit"),
    Integer::New(env->isolate(), code)
  };

  MakeCallback(env->isolate(),
               process_object, "emit", arraysize(args), args,
               {0, 0}).ToLocalChecked();

  // Reload exit code, it may be changed by `emit('exit')`
  return process_object->Get(env->context(), exit_code).ToLocalChecked()
      ->Int32Value(env->context()).ToChecked();
}


ArrayBufferAllocator* CreateArrayBufferAllocator() {
  return new ArrayBufferAllocator();
}


void FreeArrayBufferAllocator(ArrayBufferAllocator* allocator) {
  delete allocator;
}


IsolateData* CreateIsolateData(Isolate* isolate, uv_loop_t* loop) {
  return new IsolateData(isolate, loop, nullptr);
}


IsolateData* CreateIsolateData(
    Isolate* isolate,
    uv_loop_t* loop,
    MultiIsolatePlatform* platform) {
  return new IsolateData(isolate, loop, platform);
}


IsolateData* CreateIsolateData(
    Isolate* isolate,
    uv_loop_t* loop,
    MultiIsolatePlatform* platform,
    ArrayBufferAllocator* allocator) {
  return new IsolateData(isolate, loop, platform, allocator->zero_fill_field());
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
  Environment* env = new Environment(isolate_data, context,
                                     v8_platform.GetTracingAgentWriter());
  env->Start(args, exec_args, v8_is_profiling);
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
  return v8_platform.Platform();
}


MultiIsolatePlatform* CreatePlatform(
    int thread_pool_size,
    node::tracing::TracingController* tracing_controller) {
  return new NodePlatform(thread_pool_size, tracing_controller);
}


void FreePlatform(MultiIsolatePlatform* platform) {
  delete platform;
}


Local<Context> NewContext(Isolate* isolate,
                          Local<ObjectTemplate> object_template) {
  auto context = Context::New(isolate, nullptr, object_template);
  if (context.IsEmpty()) return context;
  HandleScope handle_scope(isolate);

  context->SetEmbedderData(
      ContextEmbedderIndex::kAllowWasmCodeGeneration, True(isolate));

  {
    // Run lib/internal/per_context.js
    Context::Scope context_scope(context);
    Local<String> per_context = NodePerContextSource(isolate);
    ScriptCompiler::Source per_context_src(per_context, nullptr);
    Local<Script> s = ScriptCompiler::Compile(
        context,
        &per_context_src).ToLocalChecked();
    s->Run(context).ToLocalChecked();
  }

  return context;
}


inline int Start(Isolate* isolate, IsolateData* isolate_data,
                 const std::vector<std::string>& args,
                 const std::vector<std::string>& exec_args) {
  HandleScope handle_scope(isolate);
  Local<Context> context = NewContext(isolate);
  Context::Scope context_scope(context);
  Environment env(isolate_data, context, v8_platform.GetTracingAgentWriter());
  env.Start(args, exec_args, v8_is_profiling);

  const char* path = args.size() > 1 ? args[1].c_str() : nullptr;
  StartInspector(&env, path, env.options()->debug_options);

  if (env.options()->debug_options->inspector_enabled &&
      !v8_platform.InspectorStarted(&env)) {
    return 12;  // Signal internal error.
  }

  {
    Environment::AsyncCallbackScope callback_scope(&env);
    env.async_hooks()->push_async_ids(1, 0);
    LoadEnvironment(&env);
    env.async_hooks()->pop_async_id(1);
  }

  {
    SealHandleScope seal(isolate);
    bool more;
    env.performance_state()->Mark(
        node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
    do {
      uv_run(env.event_loop(), UV_RUN_DEFAULT);

      v8_platform.DrainVMTasks(isolate);

      more = uv_loop_alive(env.event_loop());
      if (more)
        continue;

      RunBeforeExit(&env);

      // Emit `beforeExit` if the loop became alive either after emitting
      // event, or after running some callbacks.
      more = uv_loop_alive(env.event_loop());
    } while (more == true);
    env.performance_state()->Mark(
        node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
  }

  env.set_trace_sync_io(false);

  const int exit_code = EmitExit(&env);

  WaitForInspectorDisconnect(&env);

  env.set_can_call_into_js(false);
  env.stop_sub_worker_contexts();
  uv_tty_reset_mode();
  env.RunCleanup();
  RunAtExit(&env);

  v8_platform.DrainVMTasks(isolate);
  v8_platform.CancelVMTasks(isolate);
#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif

  return exit_code;
}

bool AllowWasmCodeGenerationCallback(
    Local<Context> context, Local<String>) {
  Local<Value> wasm_code_gen =
    context->GetEmbedderData(ContextEmbedderIndex::kAllowWasmCodeGeneration);
  return wasm_code_gen->IsUndefined() || wasm_code_gen->IsTrue();
}

Isolate* NewIsolate(ArrayBufferAllocator* allocator) {
  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;
#ifdef NODE_ENABLE_VTUNE_PROFILING
  params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif

  Isolate* isolate = Isolate::New(params);
  if (isolate == nullptr)
    return nullptr;

  isolate->AddMessageListener(OnMessage);
  isolate->SetAbortOnUncaughtExceptionCallback(ShouldAbortOnUncaughtException);
  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);
  isolate->SetFatalErrorHandler(OnFatalError);
  isolate->SetAllowWasmCodeGenerationCallback(AllowWasmCodeGenerationCallback);

  return isolate;
}

inline int Start(uv_loop_t* event_loop,
                 const std::vector<std::string>& args,
                 const std::vector<std::string>& exec_args) {
  std::unique_ptr<ArrayBufferAllocator, decltype(&FreeArrayBufferAllocator)>
      allocator(CreateArrayBufferAllocator(), &FreeArrayBufferAllocator);
  Isolate* const isolate = NewIsolate(allocator.get());
  if (isolate == nullptr)
    return 12;  // Signal internal error.

  {
    Mutex::ScopedLock scoped_lock(node_isolate_mutex);
    CHECK_NULL(node_isolate);
    node_isolate = isolate;
  }

  int exit_code;
  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    std::unique_ptr<IsolateData, decltype(&FreeIsolateData)> isolate_data(
        CreateIsolateData(
            isolate,
            event_loop,
            v8_platform.Platform(),
            allocator.get()),
        &FreeIsolateData);
    // TODO(addaleax): This should load a real per-Isolate option, currently
    // this is still effectively per-process.
    if (isolate_data->options()->track_heap_objects) {
      isolate->GetHeapProfiler()->StartTrackingHeapObjects(true);
    }
    exit_code =
        Start(isolate, isolate_data.get(), args, exec_args);
  }

  {
    Mutex::ScopedLock scoped_lock(node_isolate_mutex);
    CHECK_EQ(node_isolate, isolate);
    node_isolate = nullptr;
  }

  isolate->Dispose();

  return exit_code;
}

int Start(int argc, char** argv) {
  atexit([] () { uv_tty_reset_mode(); });
  PlatformInit();
  performance::performance_node_start = PERFORMANCE_NOW();

  CHECK_GT(argc, 0);

#ifdef NODE_ENABLE_LARGE_CODE_PAGES
  if (node::IsLargePagesEnabled()) {
    if (node::MapStaticCodeToLargePages() != 0) {
      fprintf(stderr, "Reverting to default page size\n");
    }
  }
#endif

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  std::vector<std::string> args(argv, argv + argc);
  std::vector<std::string> exec_args;
  // This needs to run *before* V8::Initialize().
  Init(&args, &exec_args);

#if HAVE_OPENSSL
  {
    std::string extra_ca_certs;
    if (SafeGetenv("NODE_EXTRA_CA_CERTS", &extra_ca_certs))
      crypto::UseExtraCaCerts(extra_ca_certs);
  }
#ifdef NODE_FIPS_MODE
  // In the case of FIPS builds we should make sure
  // the random source is properly initialized first.
  OPENSSL_init();
#endif  // NODE_FIPS_MODE
  // V8 on Windows doesn't have a good source of entropy. Seed it from
  // OpenSSL's pool.
  V8::SetEntropySource(crypto::EntropySource);
#endif  // HAVE_OPENSSL

  v8_platform.Initialize(
      per_process_opts->v8_thread_pool_size);
  V8::Initialize();
  performance::performance_v8_start = PERFORMANCE_NOW();
  v8_initialized = true;
  const int exit_code =
      Start(uv_default_loop(), args, exec_args);
  v8_platform.StopTracingAgent();
  v8_initialized = false;
  V8::Dispose();

  // uv_run cannot be called from the time before the beforeExit callback
  // runs until the program exits unless the event loop has any referenced
  // handles after beforeExit terminates. This prevents unrefed timers
  // that happen to terminate during shutdown from being run unsafely.
  // Since uv_run cannot be called, uv_async handles held by the platform
  // will never be fully cleaned up.
  v8_platform.Dispose();

  return exit_code;
}

// Call built-in modules' _register_<module name> function to
// do module registration explicitly.
void RegisterBuiltinModules() {
#define V(modname) _register_##modname();
  NODE_BUILTIN_MODULES(V)
#undef V
}

}  // namespace node

#if !HAVE_INSPECTOR
void Initialize() {}

NODE_BUILTIN_MODULE_CONTEXT_AWARE(inspector, Initialize)
#endif  // !HAVE_INSPECTOR

#include "node.h"
#include "node_buffer.h"
#include "node_constants.h"
#include "node_file.h"
#include "node_http_parser.h"
#include "node_javascript.h"
#include "node_version.h"
#include "node_internals.h"
#include "node_revert.h"
#include "node_debug_options.h"

#if defined HAVE_PERFCTR
#include "node_counters.h"
#endif

#if HAVE_OPENSSL
#include "node_crypto.h"
#endif

#if defined(NODE_HAVE_I18N_SUPPORT)
#include "node_i18n.h"
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
#include "node_dtrace.h"
#endif

#if defined HAVE_LTTNG
#include "node_lttng.h"
#endif

#include "ares.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "string_bytes.h"
#include "tracing/agent.h"
#include "util.h"
#include "uv.h"
#if NODE_USE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif  // NODE_USE_V8_PLATFORM
#include "v8-debug.h"
#include "v8-profiler.h"
#include "zlib.h"

#ifdef NODE_ENABLE_VTUNE_PROFILING
#include "../deps/v8/src/third_party/vtune/v8-vtune.h"
#endif

#include <errno.h>
#include <limits.h>  // PATH_MAX
#include <locale.h>
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
#define getpid GetCurrentProcessId
#define umask _umask
typedef int mode_t;
#else
#include <pthread.h>
#include <sys/resource.h>  // getrlimit, setrlimit
#include <unistd.h>  // setuid, getuid
#endif

#if defined(__POSIX__) && !defined(__ANDROID__)
#include <pwd.h>  // getpwnam()
#include <grp.h>  // getgrnam()
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#elif !defined(_MSC_VER)
extern char **environ;
#endif

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::Message;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::PromiseRejectMessage;
using v8::PropertyCallbackInfo;
using v8::ScriptOrigin;
using v8::SealHandleScope;
using v8::String;
using v8::TryCatch;
using v8::Uint32Array;
using v8::V8;
using v8::Value;

static bool print_eval = false;
static bool force_repl = false;
static bool syntax_check_only = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static bool trace_sync_io = false;
static bool track_heap_objects = false;
static const char* eval_string = nullptr;
static unsigned int preload_module_count = 0;
static const char** preload_modules = nullptr;
static const int v8_default_thread_pool_size = 4;
static int v8_thread_pool_size = v8_default_thread_pool_size;
static bool prof_process = false;
static bool v8_is_profiling = false;
static bool node_is_initialized = false;
static node_module* modpending;
static node_module* modlist_builtin;
static node_module* modlist_linked;
static node_module* modlist_addon;
static bool trace_enabled = false;
static const char* trace_enabled_categories = nullptr;

#if defined(NODE_HAVE_I18N_SUPPORT)
// Path to ICU data (for i18n / Intl)
std::string icu_data_dir;  // NOLINT(runtime/string)
#endif

// used by C++ modules as well
bool no_deprecation = false;

#if HAVE_OPENSSL
// use OpenSSL's cert store instead of bundled certs
bool ssl_openssl_cert_store =
#if defined(NODE_OPENSSL_CERT_STORE)
        true;
#else
        false;
#endif

# if NODE_FIPS_MODE
// used by crypto module
bool enable_fips_crypto = false;
bool force_fips_crypto = false;
# endif  // NODE_FIPS_MODE
std::string openssl_config;  // NOLINT(runtime/string)
#endif  // HAVE_OPENSSL

// true if process warnings should be suppressed
bool no_process_warnings = false;
bool trace_warnings = false;

// Set in node.cc by ParseArgs when --preserve-symlinks is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/module.js
bool config_preserve_symlinks = false;

// Set in node.cc by ParseArgs when --redirect-warnings= is used.
std::string config_warning_file;  // NOLINT(runtime/string)

bool v8_initialized = false;

// process-relative uptime base, initialized at start-up
static double prog_start_time;
static bool debugger_running;
static uv_async_t dispatch_debug_messages_async;

static Mutex node_isolate_mutex;
static v8::Isolate* node_isolate;

static node::DebugOptions debug_options;

static struct {
#if NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {
    platform_ = v8::platform::CreateDefaultPlatform(thread_pool_size);
    V8::InitializePlatform(platform_);
    tracing::TraceEventHelper::SetCurrentPlatform(platform_);
  }

  void PumpMessageLoop(Isolate* isolate) {
    v8::platform::PumpMessageLoop(platform_, isolate);
  }

  void Dispose() {
    delete platform_;
    platform_ = nullptr;
  }

#if HAVE_INSPECTOR
  bool StartInspector(Environment *env, const char* script_path,
                      const node::DebugOptions& options) {
    return env->inspector_agent()->Start(platform_, script_path, options);
  }
#endif  // HAVE_INSPECTOR

  void StartTracingAgent() {
    CHECK(tracing_agent_ == nullptr);
    tracing_agent_ = new tracing::Agent();
    tracing_agent_->Start(platform_, trace_enabled_categories);
  }

  void StopTracingAgent() {
    tracing_agent_->Stop();
  }

  v8::Platform* platform_;
  tracing::Agent* tracing_agent_;
#else  // !NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {}
  void PumpMessageLoop(Isolate* isolate) {}
  void Dispose() {}
  bool StartInspector(Environment *env, const char* script_path,
                      const node::DebugOptions& options) {
    env->ThrowError("Node compiled with NODE_USE_V8_PLATFORM=0");
    return false;  // make compiler happy
  }

  void StartTracingAgent() {
    fprintf(stderr, "Node compiled with NODE_USE_V8_PLATFORM=0, "
                    "so event tracing is not available.\n");
  }
  void StopTracingAgent() {}
#endif  // !NODE_USE_V8_PLATFORM
} v8_platform;

#ifdef __POSIX__
static uv_sem_t debug_semaphore;
static const unsigned kMaxSignal = 32;
#endif

static void PrintErrorString(const char* format, ...) {
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


static void CheckImmediate(uv_check_t* handle) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MakeCallback(env, env->process_object(), env->immediate_callback_string());
}


static void IdleImmediateDummy(uv_idle_t* handle) {
  // Do nothing. Only for maintaining event loop.
  // TODO(bnoordhuis) Maybe make libuv accept nullptr idle callbacks.
}


static inline const char *errno_string(int errorno) {
#define ERRNO_CASE(e)  case e: return #e;
  switch (errorno) {
#ifdef EACCES
  ERRNO_CASE(EACCES);
#endif

#ifdef EADDRINUSE
  ERRNO_CASE(EADDRINUSE);
#endif

#ifdef EADDRNOTAVAIL
  ERRNO_CASE(EADDRNOTAVAIL);
#endif

#ifdef EAFNOSUPPORT
  ERRNO_CASE(EAFNOSUPPORT);
#endif

#ifdef EAGAIN
  ERRNO_CASE(EAGAIN);
#endif

#ifdef EWOULDBLOCK
# if EAGAIN != EWOULDBLOCK
  ERRNO_CASE(EWOULDBLOCK);
# endif
#endif

#ifdef EALREADY
  ERRNO_CASE(EALREADY);
#endif

#ifdef EBADF
  ERRNO_CASE(EBADF);
#endif

#ifdef EBADMSG
  ERRNO_CASE(EBADMSG);
#endif

#ifdef EBUSY
  ERRNO_CASE(EBUSY);
#endif

#ifdef ECANCELED
  ERRNO_CASE(ECANCELED);
#endif

#ifdef ECHILD
  ERRNO_CASE(ECHILD);
#endif

#ifdef ECONNABORTED
  ERRNO_CASE(ECONNABORTED);
#endif

#ifdef ECONNREFUSED
  ERRNO_CASE(ECONNREFUSED);
#endif

#ifdef ECONNRESET
  ERRNO_CASE(ECONNRESET);
#endif

#ifdef EDEADLK
  ERRNO_CASE(EDEADLK);
#endif

#ifdef EDESTADDRREQ
  ERRNO_CASE(EDESTADDRREQ);
#endif

#ifdef EDOM
  ERRNO_CASE(EDOM);
#endif

#ifdef EDQUOT
  ERRNO_CASE(EDQUOT);
#endif

#ifdef EEXIST
  ERRNO_CASE(EEXIST);
#endif

#ifdef EFAULT
  ERRNO_CASE(EFAULT);
#endif

#ifdef EFBIG
  ERRNO_CASE(EFBIG);
#endif

#ifdef EHOSTUNREACH
  ERRNO_CASE(EHOSTUNREACH);
#endif

#ifdef EIDRM
  ERRNO_CASE(EIDRM);
#endif

#ifdef EILSEQ
  ERRNO_CASE(EILSEQ);
#endif

#ifdef EINPROGRESS
  ERRNO_CASE(EINPROGRESS);
#endif

#ifdef EINTR
  ERRNO_CASE(EINTR);
#endif

#ifdef EINVAL
  ERRNO_CASE(EINVAL);
#endif

#ifdef EIO
  ERRNO_CASE(EIO);
#endif

#ifdef EISCONN
  ERRNO_CASE(EISCONN);
#endif

#ifdef EISDIR
  ERRNO_CASE(EISDIR);
#endif

#ifdef ELOOP
  ERRNO_CASE(ELOOP);
#endif

#ifdef EMFILE
  ERRNO_CASE(EMFILE);
#endif

#ifdef EMLINK
  ERRNO_CASE(EMLINK);
#endif

#ifdef EMSGSIZE
  ERRNO_CASE(EMSGSIZE);
#endif

#ifdef EMULTIHOP
  ERRNO_CASE(EMULTIHOP);
#endif

#ifdef ENAMETOOLONG
  ERRNO_CASE(ENAMETOOLONG);
#endif

#ifdef ENETDOWN
  ERRNO_CASE(ENETDOWN);
#endif

#ifdef ENETRESET
  ERRNO_CASE(ENETRESET);
#endif

#ifdef ENETUNREACH
  ERRNO_CASE(ENETUNREACH);
#endif

#ifdef ENFILE
  ERRNO_CASE(ENFILE);
#endif

#ifdef ENOBUFS
  ERRNO_CASE(ENOBUFS);
#endif

#ifdef ENODATA
  ERRNO_CASE(ENODATA);
#endif

#ifdef ENODEV
  ERRNO_CASE(ENODEV);
#endif

#ifdef ENOENT
  ERRNO_CASE(ENOENT);
#endif

#ifdef ENOEXEC
  ERRNO_CASE(ENOEXEC);
#endif

#ifdef ENOLINK
  ERRNO_CASE(ENOLINK);
#endif

#ifdef ENOLCK
# if ENOLINK != ENOLCK
  ERRNO_CASE(ENOLCK);
# endif
#endif

#ifdef ENOMEM
  ERRNO_CASE(ENOMEM);
#endif

#ifdef ENOMSG
  ERRNO_CASE(ENOMSG);
#endif

#ifdef ENOPROTOOPT
  ERRNO_CASE(ENOPROTOOPT);
#endif

#ifdef ENOSPC
  ERRNO_CASE(ENOSPC);
#endif

#ifdef ENOSR
  ERRNO_CASE(ENOSR);
#endif

#ifdef ENOSTR
  ERRNO_CASE(ENOSTR);
#endif

#ifdef ENOSYS
  ERRNO_CASE(ENOSYS);
#endif

#ifdef ENOTCONN
  ERRNO_CASE(ENOTCONN);
#endif

#ifdef ENOTDIR
  ERRNO_CASE(ENOTDIR);
#endif

#ifdef ENOTEMPTY
# if ENOTEMPTY != EEXIST
  ERRNO_CASE(ENOTEMPTY);
# endif
#endif

#ifdef ENOTSOCK
  ERRNO_CASE(ENOTSOCK);
#endif

#ifdef ENOTSUP
  ERRNO_CASE(ENOTSUP);
#else
# ifdef EOPNOTSUPP
  ERRNO_CASE(EOPNOTSUPP);
# endif
#endif

#ifdef ENOTTY
  ERRNO_CASE(ENOTTY);
#endif

#ifdef ENXIO
  ERRNO_CASE(ENXIO);
#endif


#ifdef EOVERFLOW
  ERRNO_CASE(EOVERFLOW);
#endif

#ifdef EPERM
  ERRNO_CASE(EPERM);
#endif

#ifdef EPIPE
  ERRNO_CASE(EPIPE);
#endif

#ifdef EPROTO
  ERRNO_CASE(EPROTO);
#endif

#ifdef EPROTONOSUPPORT
  ERRNO_CASE(EPROTONOSUPPORT);
#endif

#ifdef EPROTOTYPE
  ERRNO_CASE(EPROTOTYPE);
#endif

#ifdef ERANGE
  ERRNO_CASE(ERANGE);
#endif

#ifdef EROFS
  ERRNO_CASE(EROFS);
#endif

#ifdef ESPIPE
  ERRNO_CASE(ESPIPE);
#endif

#ifdef ESRCH
  ERRNO_CASE(ESRCH);
#endif

#ifdef ESTALE
  ERRNO_CASE(ESTALE);
#endif

#ifdef ETIME
  ERRNO_CASE(ETIME);
#endif

#ifdef ETIMEDOUT
  ERRNO_CASE(ETIMEDOUT);
#endif

#ifdef ETXTBSY
  ERRNO_CASE(ETXTBSY);
#endif

#ifdef EXDEV
  ERRNO_CASE(EXDEV);
#endif

  default: return "";
  }
}

const char *signo_string(int signo) {
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


Local<Value> ErrnoException(Isolate* isolate,
                            int errorno,
                            const char *syscall,
                            const char *msg,
                            const char *path) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Value> e;
  Local<String> estring = OneByteString(env->isolate(), errno_string(errorno));
  if (msg == nullptr || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(env->isolate(), msg);

  Local<String> cons =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(env->isolate(), ", "));
  cons = String::Concat(cons, message);

  Local<String> path_string;
  if (path != nullptr) {
    // FIXME(bnoordhuis) It's questionable to interpret the file path as UTF-8.
    path_string = String::NewFromUtf8(env->isolate(), path);
  }

  if (path_string.IsEmpty() == false) {
    cons = String::Concat(cons, FIXED_ONE_BYTE_STRING(env->isolate(), " '"));
    cons = String::Concat(cons, path_string);
    cons = String::Concat(cons, FIXED_ONE_BYTE_STRING(env->isolate(), "'"));
  }
  e = Exception::Error(cons);

  Local<Object> obj = e->ToObject(env->isolate());
  obj->Set(env->errno_string(), Integer::New(env->isolate(), errorno));
  obj->Set(env->code_string(), estring);

  if (path_string.IsEmpty() == false) {
    obj->Set(env->path_string(), path_string);
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(env->isolate(), syscall));
  }

  return e;
}


static Local<String> StringFromPath(Isolate* isolate, const char* path) {
#ifdef _WIN32
  if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
    return String::Concat(FIXED_ONE_BYTE_STRING(isolate, "\\\\"),
                          String::NewFromUtf8(isolate, path + 8));
  } else if (strncmp(path, "\\\\?\\", 4) == 0) {
    return String::NewFromUtf8(isolate, path + 4);
  }
#endif

  return String::NewFromUtf8(isolate, path);
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path) {
  return UVException(isolate, errorno, syscall, msg, path, nullptr);
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path,
                         const char* dest) {
  Environment* env = Environment::GetCurrent(isolate);

  if (!msg || !msg[0])
    msg = uv_strerror(errorno);

  Local<String> js_code = OneByteString(isolate, uv_err_name(errorno));
  Local<String> js_syscall = OneByteString(isolate, syscall);
  Local<String> js_path;
  Local<String> js_dest;

  Local<String> js_msg = js_code;
  js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, ": "));
  js_msg = String::Concat(js_msg, OneByteString(isolate, msg));
  js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, ", "));
  js_msg = String::Concat(js_msg, js_syscall);

  if (path != nullptr) {
    js_path = StringFromPath(isolate, path);

    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, " '"));
    js_msg = String::Concat(js_msg, js_path);
    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  if (dest != nullptr) {
    js_dest = StringFromPath(isolate, dest);

    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, " -> '"));
    js_msg = String::Concat(js_msg, js_dest);
    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "'"));
  }

  Local<Object> e = Exception::Error(js_msg)->ToObject(isolate);

  // TODO(piscisaureus) errno should probably go; the user has no way of
  // knowing which uv errno value maps to which error.
  e->Set(env->errno_string(), Integer::New(isolate, errorno));
  e->Set(env->code_string(), js_code);
  e->Set(env->syscall_string(), js_syscall);
  if (!js_path.IsEmpty())
    e->Set(env->path_string(), js_path);
  if (!js_dest.IsEmpty())
    e->Set(env->dest_string(), js_dest);

  return e;
}


// Look up environment variable unless running as setuid root.
bool SafeGetenv(const char* key, std::string* text) {
#ifndef _WIN32
  // TODO(bnoordhuis) Should perhaps also check whether getauxval(AT_SECURE)
  // is non-zero on Linux.
  if (getuid() != geteuid() || getgid() != getegid()) {
    text->clear();
    return false;
  }
#endif
  if (const char* value = getenv(key)) {
    *text = value;
    return true;
  }
  text->clear();
  return false;
}


#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char *winapi_strerror(const int errorno, bool* must_free) {
  char *errmsg = nullptr;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, nullptr);

  if (errmsg) {
    *must_free = true;

    // Remove trailing newlines
    for (int i = strlen(errmsg) - 1;
        i >= 0 && (errmsg[i] == '\n' || errmsg[i] == '\r'); i--) {
      errmsg[i] = '\0';
    }

    return errmsg;
  } else {
    // FormatMessage failed
    *must_free = false;
    return "Unknown error";
  }
}


Local<Value> WinapiErrnoException(Isolate* isolate,
                                  int errorno,
                                  const char* syscall,
                                  const char* msg,
                                  const char* path) {
  Environment* env = Environment::GetCurrent(isolate);
  Local<Value> e;
  bool must_free = false;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno, &must_free);
  }
  Local<String> message = OneByteString(env->isolate(), msg);

  if (path) {
    Local<String> cons1 =
        String::Concat(message, FIXED_ONE_BYTE_STRING(isolate, " '"));
    Local<String> cons2 =
        String::Concat(cons1, String::NewFromUtf8(isolate, path));
    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(isolate, "'"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Object> obj = e->ToObject(env->isolate());
  obj->Set(env->errno_string(), Integer::New(isolate, errorno));

  if (path != nullptr) {
    obj->Set(env->path_string(), String::NewFromUtf8(isolate, path));
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(isolate, syscall));
  }

  if (must_free)
    LocalFree((HLOCAL)msg);

  return e;
}
#endif


void* ArrayBufferAllocator::Allocate(size_t size) {
  if (zero_fill_field_ || zero_fill_all_buffers)
    return node::UncheckedCalloc(size);
  else
    return node::UncheckedMalloc(size);
}

static bool DomainHasErrorHandler(const Environment* env,
                                  const Local<Object>& domain) {
  HandleScope scope(env->isolate());

  Local<Value> domain_event_listeners_v = domain->Get(env->events_string());
  if (!domain_event_listeners_v->IsObject())
    return false;

  Local<Object> domain_event_listeners_o =
      domain_event_listeners_v.As<Object>();

  Local<Value> domain_error_listeners_v =
      domain_event_listeners_o->Get(env->error_string());

  if (domain_error_listeners_v->IsFunction() ||
      (domain_error_listeners_v->IsArray() &&
      domain_error_listeners_v.As<Array>()->Length() > 0))
    return true;

  return false;
}

static bool DomainsStackHasErrorHandler(const Environment* env) {
  HandleScope scope(env->isolate());

  if (!env->using_domains())
    return false;

  Local<Array> domains_stack_array = env->domains_stack_array().As<Array>();
  if (domains_stack_array->Length() == 0)
    return false;

  uint32_t domains_stack_length = domains_stack_array->Length();
  for (uint32_t i = domains_stack_length; i > 0; --i) {
    Local<Value> domain_v = domains_stack_array->Get(i - 1);
    if (!domain_v->IsObject())
      return false;

    Local<Object> domain = domain_v.As<Object>();
    if (DomainHasErrorHandler(env, domain))
      return true;
  }

  return false;
}


static bool ShouldAbortOnUncaughtException(Isolate* isolate) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> process_object = env->process_object();
  Local<String> emitting_top_level_domain_error_key =
    env->emitting_top_level_domain_error_string();
  bool isEmittingTopLevelDomainError =
      process_object->Get(emitting_top_level_domain_error_key)->BooleanValue();

  return isEmittingTopLevelDomainError || !DomainsStackHasErrorHandler(env);
}


void SetupDomainUse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (env->using_domains())
    return;
  env->set_using_domains(true);

  HandleScope scope(env->isolate());
  Local<Object> process_object = env->process_object();

  Local<String> tick_callback_function_key = env->tick_domain_cb_string();
  Local<Function> tick_callback_function =
      process_object->Get(tick_callback_function_key).As<Function>();

  if (!tick_callback_function->IsFunction()) {
    fprintf(stderr, "process._tickDomainCallback assigned to non-function\n");
    ABORT();
  }

  process_object->Set(env->tick_callback_string(), tick_callback_function);
  env->set_tick_callback_function(tick_callback_function);

  CHECK(args[0]->IsArray());
  env->set_domain_array(args[0].As<Array>());

  CHECK(args[1]->IsArray());
  env->set_domains_stack_array(args[1].As<Array>());

  // Do a little housekeeping.
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupDomainUse")).FromJust();

  uint32_t* const fields = env->domain_flag()->fields();
  uint32_t const fields_count = env->domain_flag()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  args.GetReturnValue().Set(Uint32Array::New(array_buffer, 0, fields_count));
}

void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->RunMicrotasks();
}


void SetupProcessObject(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());

  env->set_push_values_to_array_function(args[0].As<Function>());
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "_setupProcessObject")).FromJust();
}


void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsObject());

  env->set_tick_callback_function(args[0].As<Function>());

  env->SetMethod(args[1].As<Object>(), "runMicrotasks", RunMicrotasks);

  // Do a little housekeeping.
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupNextTick")).FromJust();

  // Values use to cross communicate with processNextTick.
  uint32_t* const fields = env->tick_info()->fields();
  uint32_t const fields_count = env->tick_info()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  args.GetReturnValue().Set(Uint32Array::New(array_buffer, 0, fields_count));
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  Local<Value> value = message.GetValue();
  Local<Integer> event = Integer::New(isolate, message.GetEvent());

  Environment* env = Environment::GetCurrent(isolate);
  Local<Function> callback = env->promise_reject_function();

  if (value.IsEmpty())
    value = Undefined(isolate);

  Local<Value> args[] = { event, promise, value };
  Local<Object> process = env->process_object();

  callback->Call(process, arraysize(args), args);
}

void SetupPromises(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args[0]->IsFunction());

  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  env->set_promise_reject_function(args[0].As<Function>());

  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupPromises")).FromJust();
}


Local<Value> MakeCallback(Environment* env,
                          Local<Value> recv,
                          const Local<Function> callback,
                          int argc,
                          Local<Value> argv[]) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  CHECK_EQ(env->context(), env->isolate()->GetCurrentContext());

  Local<Function> pre_fn = env->async_hooks_pre_function();
  Local<Function> post_fn = env->async_hooks_post_function();
  Local<Object> object, domain;
  bool ran_init_callback = false;
  bool has_domain = false;

  Environment::AsyncCallbackScope callback_scope(env);

  // TODO(trevnorris): Adding "_asyncQueue" to the "this" in the init callback
  // is a horrible way to detect usage. Rethink how detection should happen.
  if (recv->IsObject()) {
    object = recv.As<Object>();
    Local<Value> async_queue_v = object->Get(env->async_queue_string());
    if (async_queue_v->IsObject())
      ran_init_callback = true;
  }

  if (env->using_domains()) {
    CHECK(recv->IsObject());
    Local<Value> domain_v = object->Get(env->domain_string());
    has_domain = domain_v->IsObject();
    if (has_domain) {
      domain = domain_v.As<Object>();
      if (domain->Get(env->disposed_string())->IsTrue())
        return Undefined(env->isolate());
    }
  }

  if (has_domain) {
    Local<Value> enter_v = domain->Get(env->enter_string());
    if (enter_v->IsFunction()) {
      if (enter_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::MakeCallback",
                   "domain enter callback threw, please report this");
      }
    }
  }

  if (ran_init_callback && !pre_fn.IsEmpty()) {
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar = pre_fn->Call(env->context(), object, 0, nullptr);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
      return Local<Value>();
    }
  }

  Local<Value> ret = callback->Call(recv, argc, argv);

  if (ran_init_callback && !post_fn.IsEmpty()) {
    Local<Value> did_throw = Boolean::New(env->isolate(), ret.IsEmpty());
    // Currently there's no way to retrieve an uid from node::MakeCallback().
    // This needs to be fixed.
    Local<Value> vals[] =
        { Undefined(env->isolate()).As<Value>(), did_throw };
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar =
        post_fn->Call(env->context(), object, arraysize(vals), vals);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
      return Local<Value>();
    }
  }

  if (ret.IsEmpty()) {
    // NOTE: For backwards compatibility with public API we return Undefined()
    // if the top level call threw.
    return callback_scope.in_makecallback() ?
        ret : Undefined(env->isolate()).As<Value>();
  }

  if (has_domain) {
    Local<Value> exit_v = domain->Get(env->exit_string());
    if (exit_v->IsFunction()) {
      if (exit_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::MakeCallback",
                   "domain exit callback threw, please report this");
      }
    }
  }

  if (callback_scope.in_makecallback()) {
    return ret;
  }

  Environment::TickInfo* tick_info = env->tick_info();

  if (tick_info->length() == 0) {
    env->isolate()->RunMicrotasks();
  }

  Local<Object> process = env->process_object();

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  if (env->tick_callback_function()->Call(process, 0, nullptr).IsEmpty()) {
    return Undefined(env->isolate());
  }

  return ret;
}


Local<Value> MakeCallback(Environment* env,
                           Local<Object> recv,
                           Local<String> symbol,
                           int argc,
                           Local<Value> argv[]) {
  Local<Value> cb_v = recv->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(env, recv.As<Value>(), cb_v.As<Function>(), argc, argv);
}


Local<Value> MakeCallback(Environment* env,
                           Local<Object> recv,
                           const char* method,
                           int argc,
                           Local<Value> argv[]) {
  Local<String> method_string = OneByteString(env->isolate(), method);
  return MakeCallback(env, recv, method_string, argc, argv);
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          const char* method,
                          int argc,
                          Local<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<String> method_string = OneByteString(isolate, method);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, method_string, argc, argv));
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<String> symbol,
                          int argc,
                          Local<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<Value> callback_v = recv->Get(symbol);
  if (callback_v.IsEmpty()) return Local<Value>();
  if (!callback_v->IsFunction()) return Local<Value>();
  Local<Function> callback = callback_v.As<Function>();
  return handle_scope.Escape(MakeCallback(isolate, recv, callback, argc, argv));
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<Function> callback,
                          int argc,
                          Local<Value> argv[]) {
  // Observe the following two subtleties:
  //
  // 1. The environment is retrieved from the callback function's context.
  // 2. The context to enter is retrieved from the environment.
  //
  // Because of the AssignToContext() call in src/node_contextify.cc,
  // the two contexts need not be the same.
  EscapableHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(callback->CreationContext());
  Context::Scope context_scope(env->context());
  return handle_scope.Escape(
      MakeCallback(env, recv.As<Value>(), callback, argc, argv));
}


enum encoding ParseEncoding(const char* encoding,
                            enum encoding default_encoding) {
  switch (encoding[0]) {
    case 'u':
      // utf8, utf16le
      if (encoding[1] == 't' && encoding[2] == 'f') {
        // Skip `-`
        encoding += encoding[3] == '-' ? 4 : 3;
        if (encoding[0] == '8' && encoding[1] == '\0')
          return UTF8;
        if (strncmp(encoding, "16le", 4) == 0)
          return UCS2;

      // ucs2
      } else if (encoding[1] == 'c' && encoding[2] == 's') {
        encoding += encoding[3] == '-' ? 4 : 3;
        if (encoding[0] == '2' && encoding[1] == '\0')
          return UCS2;
      }
      break;
    case 'l':
      // latin1
      if (encoding[1] == 'a') {
        if (strncmp(encoding + 2, "tin1", 4) == 0)
          return LATIN1;
      }
      break;
    case 'b':
      // binary
      if (encoding[1] == 'i') {
        if (strncmp(encoding + 2, "nary", 4) == 0)
          return LATIN1;

      // buffer
      } else if (encoding[1] == 'u') {
        if (strncmp(encoding + 2, "ffer", 4) == 0)
          return BUFFER;
      }
      break;
    case '\0':
      return default_encoding;
    default:
      break;
  }

  if (StringEqualNoCase(encoding, "utf8")) {
    return UTF8;
  } else if (StringEqualNoCase(encoding, "utf-8")) {
    return UTF8;
  } else if (StringEqualNoCase(encoding, "ascii")) {
    return ASCII;
  } else if (StringEqualNoCase(encoding, "base64")) {
    return BASE64;
  } else if (StringEqualNoCase(encoding, "ucs2")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "ucs-2")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "utf16le")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "utf-16le")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "latin1")) {
    return LATIN1;
  } else if (StringEqualNoCase(encoding, "binary")) {
    return LATIN1;  // BINARY is a deprecated alias of LATIN1.
  } else if (StringEqualNoCase(encoding, "buffer")) {
    return BUFFER;
  } else if (StringEqualNoCase(encoding, "hex")) {
    return HEX;
  } else {
    return default_encoding;
  }
}


enum encoding ParseEncoding(Isolate* isolate,
                            Local<Value> encoding_v,
                            enum encoding default_encoding) {
  if (!encoding_v->IsString())
    return default_encoding;

  node::Utf8Value encoding(isolate, encoding_v);

  return ParseEncoding(*encoding, default_encoding);
}

Local<Value> Encode(Isolate* isolate,
                    const char* buf,
                    size_t len,
                    enum encoding encoding) {
  CHECK_NE(encoding, UCS2);
  return StringBytes::Encode(isolate, buf, len, encoding);
}

Local<Value> Encode(Isolate* isolate, const uint16_t* buf, size_t len) {
  return StringBytes::Encode(isolate, buf, len);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(Isolate* isolate,
                    Local<Value> val,
                    enum encoding encoding) {
  HandleScope scope(isolate);

  return StringBytes::Size(isolate, val, encoding);
}

// Returns number of bytes written.
ssize_t DecodeWrite(Isolate* isolate,
                    char* buf,
                    size_t buflen,
                    Local<Value> val,
                    enum encoding encoding) {
  return StringBytes::Write(isolate, buf, buflen, val, encoding, nullptr);
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

    auto context = env->context();
    auto processed_private_symbol = env->processed_private_symbol();
    // Do it only once per message
    if (err_obj->HasPrivate(context, processed_private_symbol).FromJust())
      return;
    err_obj->SetPrivate(
        context,
        processed_private_symbol,
        True(env->isolate()));
  }

  // Print (filename):(line number): (message).
  node::Utf8Value filename(env->isolate(), message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber();
  // Print line of source code.
  node::Utf8Value sourceline(env->isolate(), message->GetSourceLine());
  const char* sourceline_string = *sourceline;

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

  int start = message->GetStartColumn(env->context()).FromMaybe(0);
  int end = message->GetEndColumn(env->context()).FromMaybe(0);

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

  Local<String> arrow_str = String::NewFromUtf8(env->isolate(), arrow);

  const bool can_set_arrow = !arrow_str.IsEmpty() && !err_obj.IsEmpty();
  // If allocating arrow_str failed, print it out. There's not much else to do.
  // If it's not an error, but something needs to be printed out because
  // it's a fatal exception, also print it out from here.
  // Otherwise, the arrow property will be attached to the object and handled
  // by the caller.
  if (!can_set_arrow || (mode == FATAL_ERROR && !err_obj->IsNativeError())) {
    if (env->printed_error())
      return;
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


static void ReportException(Environment* env,
                            Local<Value> er,
                            Local<Message> message) {
  HandleScope scope(env->isolate());

  AppendExceptionLine(env, er, message, FATAL_ERROR);

  Local<Value> trace_value;
  Local<Value> arrow;
  const bool decorated = IsExceptionDecorated(env, er);

  if (er->IsUndefined() || er->IsNull()) {
    trace_value = Undefined(env->isolate());
  } else {
    Local<Object> err_obj = er->ToObject(env->isolate());

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
      String::Utf8Value message(er);

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
}


static void ReportException(Environment* env, const TryCatch& try_catch) {
  ReportException(env, try_catch.Exception(), try_catch.Message());
}


// Executes a str within the current v8 context.
static Local<Value> ExecuteString(Environment* env,
                                  Local<String> source,
                                  Local<String> filename) {
  EscapableHandleScope scope(env->isolate());
  TryCatch try_catch(env->isolate());

  // try_catch must be nonverbose to disable FatalException() handler,
  // we will handle exceptions ourself.
  try_catch.SetVerbose(false);

  ScriptOrigin origin(filename);
  MaybeLocal<v8::Script> script =
      v8::Script::Compile(env->context(), source, &origin);
  if (script.IsEmpty()) {
    ReportException(env, try_catch);
    exit(3);
  }

  Local<Value> result = script.ToLocalChecked()->Run();
  if (result.IsEmpty()) {
    ReportException(env, try_catch);
    exit(4);
  }

  return scope.Escape(result);
}


static void GetActiveRequests(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Array> ary = Array::New(args.GetIsolate());
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

  for (auto w : *env->req_wrap_queue()) {
    if (w->persistent().IsEmpty())
      continue;
    argv[idx] = w->object();
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, ary, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }

  if (idx > 0) {
    fn->Call(ctx, ary, idx, argv).ToLocalChecked();
  }

  args.GetReturnValue().Set(ary);
}


// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
void GetActiveHandles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Array> ary = Array::New(env->isolate());
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

  Local<String> owner_sym = env->owner_string();

  for (auto w : *env->handle_wrap_queue()) {
    if (w->persistent().IsEmpty() || !HandleWrap::HasRef(w))
      continue;
    Local<Object> object = w->object();
    Local<Value> owner = object->Get(owner_sym);
    if (owner->IsUndefined())
      owner = object;
    argv[idx] = owner;
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, ary, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }
  if (idx > 0) {
    fn->Call(ctx, ary, idx, argv).ToLocalChecked();
  }

  args.GetReturnValue().Set(ary);
}


NO_RETURN void Abort() {
  DumpBacktrace(stderr);
  fflush(stderr);
  ABORT_NO_BACKTRACE();
}


NO_RETURN void Assert(const char* const (*args)[4]) {
  auto filename = (*args)[0];
  auto linenum = (*args)[1];
  auto message = (*args)[2];
  auto function = (*args)[3];

  char exepath[256];
  size_t exepath_size = sizeof(exepath);
  if (uv_exepath(exepath, &exepath_size))
    snprintf(exepath, sizeof(exepath), "node");

  char pid[12] = {0};
#ifndef _WIN32
  snprintf(pid, sizeof(pid), "[%u]", getpid());
#endif

  fprintf(stderr, "%s%s: %s:%s:%s%s Assertion `%s' failed.\n",
          exepath, pid, filename, linenum,
          function, *function ? ":" : "", message);
  fflush(stderr);

  Abort();
}


static void Abort(const FunctionCallbackInfo<Value>& args) {
  Abort();
}


static void Chdir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 1 || !args[0]->IsString()) {
    return env->ThrowTypeError("Bad argument.");
  }

  node::Utf8Value path(args.GetIsolate(), args[0]);
  int err = uv_chdir(*path);
  if (err) {
    return env->ThrowUVException(err, "uv_chdir");
  }
}


static void Cwd(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#ifdef _WIN32
  /* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
  char buf[MAX_PATH * 4];
#else
  char buf[PATH_MAX];
#endif

  size_t cwd_len = sizeof(buf);
  int err = uv_cwd(buf, &cwd_len);
  if (err) {
    return env->ThrowUVException(err, "uv_cwd");
  }

  Local<String> cwd = String::NewFromUtf8(env->isolate(),
                                          buf,
                                          String::kNormalString,
                                          cwd_len);
  args.GetReturnValue().Set(cwd);
}


static void Umask(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uint32_t old;

  if (args.Length() < 1 || args[0]->IsUndefined()) {
    old = umask(0);
    umask(static_cast<mode_t>(old));
  } else if (!args[0]->IsInt32() && !args[0]->IsString()) {
    return env->ThrowTypeError("argument must be an integer or octal string.");
  } else {
    int oct;
    if (args[0]->IsInt32()) {
      oct = args[0]->Uint32Value();
    } else {
      oct = 0;
      node::Utf8Value str(env->isolate(), args[0]);

      // Parse the octal string.
      for (size_t i = 0; i < str.length(); i++) {
        char c = (*str)[i];
        if (c > '7' || c < '0') {
          return env->ThrowTypeError("invalid octal string");
        }
        oct *= 8;
        oct += c - '0';
      }
    }
    old = umask(static_cast<mode_t>(oct));
  }

  args.GetReturnValue().Set(old);
}


#if defined(__POSIX__) && !defined(__ANDROID__)

static const uid_t uid_not_found = static_cast<uid_t>(-1);
static const gid_t gid_not_found = static_cast<gid_t>(-1);


static uid_t uid_by_name(const char* name) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getpwnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr) {
    return pp->pw_uid;
  }

  return uid_not_found;
}


static char* name_by_uid(uid_t uid) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getpwuid_r(uid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->pw_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return nullptr;
}


static gid_t gid_by_name(const char* name) {
  struct group pwd;
  struct group* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getgrnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr) {
    return pp->gr_gid;
  }

  return gid_not_found;
}


#if 0  // For future use.
static const char* name_by_gid(gid_t gid) {
  struct group pwd;
  struct group* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getgrgid_r(gid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->gr_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return nullptr;
}
#endif


static uid_t uid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<uid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(isolate, value);
    return uid_by_name(*name);
  }
}


static gid_t gid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<gid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(isolate, value);
    return gid_by_name(*name);
  }
}

static void GetUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getuid()));
}


static void GetGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getgid()));
}


static void GetEUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(geteuid()));
}


static void GetEGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getegid()));
}


static void SetGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("setgid argument must be a number or a string");
  }

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    return env->ThrowError("setgid group id does not exist");
  }

  if (setgid(gid)) {
    return env->ThrowErrnoException(errno, "setgid");
  }
}


static void SetEGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("setegid argument must be a number or string");
  }

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    return env->ThrowError("setegid group id does not exist");
  }

  if (setegid(gid)) {
    return env->ThrowErrnoException(errno, "setegid");
  }
}


static void SetUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("setuid argument must be a number or a string");
  }

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    return env->ThrowError("setuid user id does not exist");
  }

  if (setuid(uid)) {
    return env->ThrowErrnoException(errno, "setuid");
  }
}


static void SetEUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("seteuid argument must be a number or string");
  }

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    return env->ThrowError("seteuid user id does not exist");
  }

  if (seteuid(uid)) {
    return env->ThrowErrnoException(errno, "seteuid");
  }
}


static void GetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int ngroups = getgroups(0, nullptr);

  if (ngroups == -1) {
    return env->ThrowErrnoException(errno, "getgroups");
  }

  gid_t* groups = new gid_t[ngroups];

  ngroups = getgroups(ngroups, groups);

  if (ngroups == -1) {
    delete[] groups;
    return env->ThrowErrnoException(errno, "getgroups");
  }

  Local<Array> groups_list = Array::New(env->isolate(), ngroups);
  bool seen_egid = false;
  gid_t egid = getegid();

  for (int i = 0; i < ngroups; i++) {
    groups_list->Set(i, Integer::New(env->isolate(), groups[i]));
    if (groups[i] == egid)
      seen_egid = true;
  }

  delete[] groups;

  if (seen_egid == false) {
    groups_list->Set(ngroups, Integer::New(env->isolate(), egid));
  }

  args.GetReturnValue().Set(groups_list);
}


static void SetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsArray()) {
    return env->ThrowTypeError("argument 1 must be an array");
  }

  Local<Array> groups_list = args[0].As<Array>();
  size_t size = groups_list->Length();
  gid_t* groups = new gid_t[size];

  for (size_t i = 0; i < size; i++) {
    gid_t gid = gid_by_name(env->isolate(), groups_list->Get(i));

    if (gid == gid_not_found) {
      delete[] groups;
      return env->ThrowError("group name not found");
    }

    groups[i] = gid;
  }

  int rc = setgroups(size, groups);
  delete[] groups;

  if (rc == -1) {
    return env->ThrowErrnoException(errno, "setgroups");
  }
}


static void InitGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("argument 1 must be a number or a string");
  }

  if (!args[1]->IsUint32() && !args[1]->IsString()) {
    return env->ThrowTypeError("argument 2 must be a number or a string");
  }

  node::Utf8Value arg0(env->isolate(), args[0]);
  gid_t extra_group;
  bool must_free;
  char* user;

  if (args[0]->IsUint32()) {
    user = name_by_uid(args[0]->Uint32Value());
    must_free = true;
  } else {
    user = *arg0;
    must_free = false;
  }

  if (user == nullptr) {
    return env->ThrowError("initgroups user not found");
  }

  extra_group = gid_by_name(env->isolate(), args[1]);

  if (extra_group == gid_not_found) {
    if (must_free)
      free(user);
    return env->ThrowError("initgroups extra group not found");
  }

  int rc = initgroups(user, extra_group);

  if (must_free) {
    free(user);
  }

  if (rc) {
    return env->ThrowErrnoException(errno, "initgroups");
  }
}

#endif  // __POSIX__ && !defined(__ANDROID__)


static void WaitForInspectorDisconnect(Environment* env) {
#if HAVE_INSPECTOR
  if (env->inspector_agent()->IsConnected()) {
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


void Exit(const FunctionCallbackInfo<Value>& args) {
  WaitForInspectorDisconnect(Environment::GetCurrent(args));
  exit(args[0]->Int32Value());
}


static void Uptime(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  double uptime;

  uv_update_time(env->event_loop());
  uptime = uv_now(env->event_loop()) - prog_start_time;

  args.GetReturnValue().Set(Number::New(env->isolate(), uptime / 1000));
}


void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  size_t rss;
  int err = uv_resident_set_memory(&rss);
  if (err) {
    return env->ThrowUVException(err, "uv_resident_set_memory");
  }

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  env->isolate()->GetHeapStatistics(&v8_heap_stats);

  Local<Number> heap_total =
      Number::New(env->isolate(), v8_heap_stats.total_heap_size());
  Local<Number> heap_used =
      Number::New(env->isolate(), v8_heap_stats.used_heap_size());
  Local<Number> external_mem =
      Number::New(env->isolate(),
                  env->isolate()->AdjustAmountOfExternalAllocatedMemory(0));

  Local<Object> info = Object::New(env->isolate());
  info->Set(env->rss_string(), Number::New(env->isolate(), rss));
  info->Set(env->heap_total_string(), heap_total);
  info->Set(env->heap_used_string(), heap_used);
  info->Set(env->external_string(), external_mem);

  args.GetReturnValue().Set(info);
}


void Kill(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 2) {
    return env->ThrowError("Bad argument.");
  }

  int pid = args[0]->Int32Value();
  int sig = args[1]->Int32Value();
  int err = uv_kill(pid, sig);
  args.GetReturnValue().Set(err);
}

// used in Hrtime() below
#define NANOS_PER_SEC 1000000000

// Hrtime exposes libuv's uv_hrtime() high-resolution timer.
// The value returned by uv_hrtime() is a 64-bit int representing nanoseconds,
// so this function instead fills in an Uint32Array with 3 entries,
// to avoid any integer overflow possibility.
// The first two entries contain the second part of the value
// broken into the upper/lower 32 bits to be converted back in JS,
// because there is no Uint64Array in JS.
// The third entry contains the remaining nanosecond part of the value.
void Hrtime(const FunctionCallbackInfo<Value>& args) {
  uint64_t t = uv_hrtime();

  Local<ArrayBuffer> ab = args[0].As<Uint32Array>()->Buffer();
  uint32_t* fields = static_cast<uint32_t*>(ab->GetContents().Data());

  fields[0] = (t / NANOS_PER_SEC) >> 32;
  fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
  fields[2] = t % NANOS_PER_SEC;
}

// Microseconds in a second, as a float, used in CPUUsage() below
#define MICROS_PER_SEC 1e6

// CPUUsage use libuv's uv_getrusage() this-process resource usage accessor,
// to access ru_utime (user CPU time used) and ru_stime (system CPU time used),
// which are uv_timeval_t structs (long tv_sec, long tv_usec).
// Returns those values as Float64 microseconds in the elements of the array
// passed to the function.
void CPUUsage(const FunctionCallbackInfo<Value>& args) {
  uv_rusage_t rusage;

  // Call libuv to get the values we'll return.
  int err = uv_getrusage(&rusage);
  if (err) {
    // On error, return the strerror version of the error code.
    Local<String> errmsg = OneByteString(args.GetIsolate(), uv_strerror(err));
    args.GetReturnValue().Set(errmsg);
    return;
  }

  // Get the double array pointer from the Float64Array argument.
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 2);
  Local<ArrayBuffer> ab = array->Buffer();
  double* fields = static_cast<double*>(ab->GetContents().Data());

  // Set the Float64Array elements to be user / system values in microseconds.
  fields[0] = MICROS_PER_SEC * rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec;
  fields[1] = MICROS_PER_SEC * rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec;
}

extern "C" void node_module_register(void* m) {
  struct node_module* mp = reinterpret_cast<struct node_module*>(m);

  if (mp->nm_flags & NM_F_BUILTIN) {
    mp->nm_link = modlist_builtin;
    modlist_builtin = mp;
  } else if (!node_is_initialized) {
    // "Linked" modules are included as part of the node project.
    // Like builtins they are registered *before* node::Init runs.
    mp->nm_flags = NM_F_LINKED;
    mp->nm_link = modlist_linked;
    modlist_linked = mp;
  } else {
    modpending = mp;
  }
}

struct node_module* get_builtin_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_builtin; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & NM_F_BUILTIN) != 0);
  return (mp);
}

struct node_module* get_linked_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_linked; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & NM_F_LINKED) != 0);
  return mp;
}

typedef void (UV_DYNAMIC* extInit)(Local<Object> exports);

// DLOpen is process.dlopen(module, filename).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_lib_t lib;

  CHECK_EQ(modpending, nullptr);

  if (args.Length() != 2) {
    env->ThrowError("process.dlopen takes exactly 2 arguments.");
    return;
  }

  Local<Object> module = args[0]->ToObject(env->isolate());  // Cast
  node::Utf8Value filename(env->isolate(), args[1]);  // Cast
  const bool is_dlopen_error = uv_dlopen(*filename, &lib);

  // Objects containing v14 or later modules will have registered themselves
  // on the pending list.  Activate all of them now.  At present, only one
  // module per object is supported.
  node_module* const mp = modpending;
  modpending = nullptr;

  if (is_dlopen_error) {
    Local<String> errmsg = OneByteString(env->isolate(), uv_dlerror(&lib));
    uv_dlclose(&lib);
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, args[1]->ToString(env->isolate()));
#endif  // _WIN32
    env->isolate()->ThrowException(Exception::Error(errmsg));
    return;
  }

  if (mp == nullptr) {
    uv_dlclose(&lib);
    env->ThrowError("Module did not self-register.");
    return;
  }
  if (mp->nm_version != NODE_MODULE_VERSION) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "The module '%s'"
             "\nwas compiled against a different Node.js version using"
             "\nNODE_MODULE_VERSION %d. This version of Node.js requires"
             "\nNODE_MODULE_VERSION %d. Please try re-compiling or "
             "re-installing\nthe module (for instance, using `npm rebuild` or "
             "`npm install`).",
             *filename, mp->nm_version, NODE_MODULE_VERSION);

    // NOTE: `mp` is allocated inside of the shared library's memory, calling
    // `uv_dlclose` will deallocate it
    uv_dlclose(&lib);
    env->ThrowError(errmsg);
    return;
  }
  if (mp->nm_flags & NM_F_BUILTIN) {
    uv_dlclose(&lib);
    env->ThrowError("Built-in module self-registered.");
    return;
  }

  mp->nm_dso_handle = lib.handle;
  mp->nm_link = modlist_addon;
  modlist_addon = mp;

  Local<String> exports_string = env->exports_string();
  Local<Object> exports = module->Get(exports_string)->ToObject(env->isolate());

  if (mp->nm_context_register_func != nullptr) {
    mp->nm_context_register_func(exports, module, env->context(), mp->nm_priv);
  } else if (mp->nm_register_func != nullptr) {
    mp->nm_register_func(exports, module, mp->nm_priv);
  } else {
    uv_dlclose(&lib);
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


NO_RETURN void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to suppress compiler warning
  ABORT();
}


void FatalException(Isolate* isolate,
                    Local<Value> error,
                    Local<Message> message) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Function> fatal_exception_function =
      process_object->Get(fatal_exception_string).As<Function>();

  int exit_code = 0;
  if (!fatal_exception_function->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(env, error, message);
    exit_code = 6;
  }

  if (exit_code == 0) {
    TryCatch fatal_try_catch(isolate);

    // Do not call FatalException when _fatalException handler throws
    fatal_try_catch.SetVerbose(false);

    // this will return true if the JS layer handled it, false otherwise
    Local<Value> caught =
        fatal_exception_function->Call(process_object, 1, &error);

    if (fatal_try_catch.HasCaught()) {
      // the fatal exception function threw, so we must exit
      ReportException(env, fatal_try_catch);
      exit_code = 7;
    }

    if (exit_code == 0 && false == caught->BooleanValue()) {
      ReportException(env, error, message);
      exit_code = 1;
    }
  }

  if (exit_code) {
#if HAVE_INSPECTOR
    if (debug_options.inspector_enabled()) {
      env->inspector_agent()->FatalException(error, message);
    }
#endif
    exit(exit_code);
  }
}


void FatalException(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope scope(isolate);
  // TODO(bajtos) do not call FatalException if try_catch is verbose
  // (requires V8 API to expose getter for try_catch.is_verbose_)
  FatalException(isolate, try_catch.Exception(), try_catch.Message());
}


void OnMessage(Local<Message> message, Local<Value> error) {
  // The current version of V8 sends messages for errors only
  // (thus `error` is always set).
  FatalException(Isolate::GetCurrent(), error, message);
}


void ClearFatalExceptionHandlers(Environment* env) {
  Local<Object> process = env->process_object();
  Local<Value> events =
      process->Get(env->context(), env->events_string()).ToLocalChecked();

  if (events->IsObject()) {
    events.As<Object>()->Set(
        env->context(),
        OneByteString(env->isolate(), "uncaughtException"),
        Undefined(env->isolate())).FromJust();
  }

  process->Set(
      env->context(),
      env->domain_string(),
      Undefined(env->isolate())).FromJust();
}

// Call process.emitWarning(str), fmt is a snprintf() format string
void ProcessEmitWarning(Environment* env, const char* fmt, ...) {
  char warning[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(warning, sizeof(warning), fmt, ap);
  va_end(ap);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> process = env->process_object();
  MaybeLocal<Value> emit_warning = process->Get(env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "emitWarning"));
  Local<Value> arg = node::OneByteString(env->isolate(), warning);

  Local<Value> f;

  if (!emit_warning.ToLocal(&f)) return;
  if (!f->IsFunction()) return;

  // MakeCallback() unneeded, because emitWarning is internal code, it calls
  // process.emit('warning', ..), but does so on the nextTick.
  f.As<v8::Function>()->Call(process, 1, &arg);
}


static void Binding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<String> module = args[0]->ToString(env->isolate());
  node::Utf8Value module_v(env->isolate(), module);

  Local<Object> cache = env->binding_cache_object();
  Local<Object> exports;

  if (cache->Has(env->context(), module).FromJust()) {
    exports = cache->Get(module)->ToObject(env->isolate());
    args.GetReturnValue().Set(exports);
    return;
  }

  // Append a string to process.moduleLoadList
  char buf[1024];
  snprintf(buf, sizeof(buf), "Binding %s", *module_v);

  Local<Array> modules = env->module_load_list_array();
  uint32_t l = modules->Length();
  modules->Set(l, OneByteString(env->isolate(), buf));

  node_module* mod = get_builtin_module(*module_v);
  if (mod != nullptr) {
    exports = Object::New(env->isolate());
    // Internal bindings don't have a "module" object, only exports.
    CHECK_EQ(mod->nm_register_func, nullptr);
    CHECK_NE(mod->nm_context_register_func, nullptr);
    Local<Value> unused = Undefined(env->isolate());
    mod->nm_context_register_func(exports, unused,
      env->context(), mod->nm_priv);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New(env->isolate());
    DefineConstants(env->isolate(), exports);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "natives")) {
    exports = Object::New(env->isolate());
    DefineJavaScript(env, exports);
    cache->Set(module, exports);
  } else {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "No such module: %s",
             *module_v);
    return env->ThrowError(errmsg);
  }

  args.GetReturnValue().Set(exports);
}

static void LinkedBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  Local<String> module_name = args[0]->ToString(env->isolate());

  Local<Object> cache = env->binding_cache_object();
  Local<Value> exports_v = cache->Get(module_name);

  if (exports_v->IsObject())
    return args.GetReturnValue().Set(exports_v.As<Object>());

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
  Local<String> exports_prop = String::NewFromUtf8(env->isolate(), "exports");
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
  cache->Set(module_name, effective_exports);

  args.GetReturnValue().Set(effective_exports);
}

static void ProcessTitleGetter(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  char buffer[512];
  uv_get_process_title(buffer, sizeof(buffer));
  info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), buffer));
}


static void ProcessTitleSetter(Local<Name> property,
                               Local<Value> value,
                               const PropertyCallbackInfo<void>& info) {
  node::Utf8Value title(info.GetIsolate(), value);
  // TODO(piscisaureus): protect with a lock
  uv_set_process_title(*title);
}


static void EnvGetter(Local<Name> property,
                      const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  if (property->IsSymbol()) {
    return info.GetReturnValue().SetUndefined();
  }
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
  const char* val = getenv(*key);
  if (val) {
    return info.GetReturnValue().Set(String::NewFromUtf8(isolate, val));
  }
#else  // _WIN32
  node::TwoByteValue key(isolate, property);
  WCHAR buffer[32767];  // The maximum size allowed for environment variables.
  DWORD result = GetEnvironmentVariableW(reinterpret_cast<WCHAR*>(*key),
                                         buffer,
                                         arraysize(buffer));
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // not found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) &&
      result < arraysize(buffer)) {
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(buffer);
    Local<String> rc = String::NewFromTwoByte(isolate, two_byte_buffer);
    return info.GetReturnValue().Set(rc);
  }
#endif
}


static void EnvSetter(Local<Name> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
  node::Utf8Value val(info.GetIsolate(), value);
  setenv(*key, *val, 1);
#else  // _WIN32
  node::TwoByteValue key(info.GetIsolate(), property);
  node::TwoByteValue val(info.GetIsolate(), value);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  // Environment variables that start with '=' are read-only.
  if (key_ptr[0] != L'=') {
    SetEnvironmentVariableW(key_ptr, reinterpret_cast<WCHAR*>(*val));
  }
#endif
  // Whether it worked or not, always return value.
  info.GetReturnValue().Set(value);
}


static void EnvQuery(Local<Name> property,
                     const PropertyCallbackInfo<Integer>& info) {
  int32_t rc = -1;  // Not found unless proven otherwise.
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
  if (getenv(*key))
    rc = 0;
#else  // _WIN32
  node::TwoByteValue key(info.GetIsolate(), property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (GetEnvironmentVariableW(key_ptr, nullptr, 0) > 0 ||
      GetLastError() == ERROR_SUCCESS) {
    rc = 0;
    if (key_ptr[0] == L'=') {
      // Environment variables that start with '=' are hidden and read-only.
      rc = static_cast<int32_t>(v8::ReadOnly) |
           static_cast<int32_t>(v8::DontDelete) |
           static_cast<int32_t>(v8::DontEnum);
    }
  }
#endif
  if (rc != -1)
    info.GetReturnValue().Set(rc);
}


static void EnvDeleter(Local<Name> property,
                       const PropertyCallbackInfo<Boolean>& info) {
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
  unsetenv(*key);
#else
  node::TwoByteValue key(info.GetIsolate(), property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetEnvironmentVariableW(key_ptr, nullptr);
#endif

  // process.env never has non-configurable properties, so always
  // return true like the tc39 delete operator.
  info.GetReturnValue().Set(true);
}


static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

#ifdef __POSIX__
  int size = 0;
  while (environ[size])
    size++;

  Local<Array> envarr = Array::New(isolate);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    argv[idx] = String::NewFromUtf8(isolate,
                                    var,
                                    String::kNormalString,
                                    length);
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }
  if (idx > 0) {
    fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
  }
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == nullptr)
    return;  // This should not happen.
  Local<Array> envarr = Array::New(isolate);
  WCHAR* p = environment;
  while (*p) {
    WCHAR *s;
    if (*p == L'=') {
      // If the key starts with '=' it is a hidden environment variable.
      p += wcslen(p) + 1;
      continue;
    } else {
      s = wcschr(p, L'=');
    }
    if (!s) {
      s = p + wcslen(p);
    }
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(p);
    const size_t two_byte_buffer_len = s - p;
    argv[idx] = String::NewFromTwoByte(isolate,
                                       two_byte_buffer,
                                       String::kNormalString,
                                       two_byte_buffer_len);
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
      idx = 0;
    }
    p = s + wcslen(s) + 1;
  }
  if (idx > 0) {
    fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
  }
  FreeEnvironmentStringsW(environment);
#endif

  info.GetReturnValue().Set(envarr);
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

#ifdef OPENSSL_NPN_NEGOTIATED
  Local<Boolean> tls_npn = True(env->isolate());
#else
  Local<Boolean> tls_npn = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_npn"), tls_npn);

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
  Local<Boolean> tls_alpn = True(env->isolate());
#else
  Local<Boolean> tls_alpn = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_alpn"), tls_alpn);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  Local<Boolean> tls_sni = True(env->isolate());
#else
  Local<Boolean> tls_sni = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_sni"), tls_sni);

#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  Local<Boolean> tls_ocsp = True(env->isolate());
#else
  Local<Boolean> tls_ocsp = False(env->isolate());
#endif  // !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls_ocsp"), tls_ocsp);

  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "tls"),
           Boolean::New(env->isolate(),
                        get_builtin_module("crypto") != nullptr));

  return scope.Escape(obj);
}


static void DebugPortGetter(Local<Name> property,
                            const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(debug_options.port());
}


static void DebugPortSetter(Local<Name> property,
                            Local<Value> value,
                            const PropertyCallbackInfo<void>& info) {
  debug_options.set_port(value->Int32Value());
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args);
static void DebugPause(const FunctionCallbackInfo<Value>& args);
static void DebugEnd(const FunctionCallbackInfo<Value>& args);


void NeedImmediateCallbackGetter(Local<Name> property,
                                 const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  const uv_check_t* immediate_check_handle = env->immediate_check_handle();
  bool active = uv_is_active(
      reinterpret_cast<const uv_handle_t*>(immediate_check_handle));
  info.GetReturnValue().Set(active);
}


static void NeedImmediateCallbackSetter(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info) {
  Environment* env = Environment::GetCurrent(info);

  uv_check_t* immediate_check_handle = env->immediate_check_handle();
  bool active = uv_is_active(
      reinterpret_cast<const uv_handle_t*>(immediate_check_handle));

  if (active == value->BooleanValue())
    return;

  uv_idle_t* immediate_idle_handle = env->immediate_idle_handle();

  if (active) {
    uv_check_stop(immediate_check_handle);
    uv_idle_stop(immediate_idle_handle);
  } else {
    uv_check_start(immediate_check_handle, CheckImmediate);
    // Idle handle is needed only to stop the event loop from blocking in poll.
    uv_idle_start(immediate_idle_handle, IdleImmediateDummy);
  }
}


void StartProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->StartProfilerIdleNotifier();
}


void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->StopProfilerIdleNotifier();
}


#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           v8::ReadOnly).FromJust();                          \
  } while (0)

#define READONLY_DONT_ENUM_PROPERTY(obj, str, var)                            \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           static_cast<v8::PropertyAttribute>(v8::ReadOnly |  \
                                                              v8::DontEnum))  \
        .FromJust();                                                          \
  } while (0)


void SetupProcessObject(Environment* env,
                        int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv) {
  HandleScope scope(env->isolate());

  Local<Object> process = env->process_object();

  auto title_string = FIXED_ONE_BYTE_STRING(env->isolate(), "title");
  CHECK(process->SetAccessor(env->context(),
                             title_string,
                             ProcessTitleGetter,
                             ProcessTitleSetter,
                             env->as_external()).FromJust());

  // process.version
  READONLY_PROPERTY(process,
                    "version",
                    FIXED_ONE_BYTE_STRING(env->isolate(), NODE_VERSION));

  // process.moduleLoadList
  READONLY_PROPERTY(process,
                    "moduleLoadList",
                    env->module_load_list_array());

  // process.versions
  Local<Object> versions = Object::New(env->isolate());
  READONLY_PROPERTY(process, "versions", versions);

  const char http_parser_version[] = NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
                                     "."
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)
                                     "."
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);
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
                    "ares",
                    FIXED_ONE_BYTE_STRING(env->isolate(), ARES_VERSION_STR));

  const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);
  READONLY_PROPERTY(
      versions,
      "modules",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_modules_version));

  // process._promiseRejectEvent
  Local<Object> promiseRejectEvent = Object::New(env->isolate());
  READONLY_DONT_ENUM_PROPERTY(process,
                              "_promiseRejectEvent",
                              promiseRejectEvent);
  READONLY_PROPERTY(promiseRejectEvent,
                    "unhandled",
                    Integer::New(env->isolate(),
                                 v8::kPromiseRejectWithNoHandler));
  READONLY_PROPERTY(promiseRejectEvent,
                    "handled",
                    Integer::New(env->isolate(),
                                 v8::kPromiseHandlerAddedAfterReject));

#if HAVE_OPENSSL
  // Stupid code to slice out the version string.
  {  // NOLINT(whitespace/braces)
    size_t i, j, k;
    int c;
    for (i = j = 0, k = sizeof(OPENSSL_VERSION_TEXT) - 1; i < k; ++i) {
      c = OPENSSL_VERSION_TEXT[i];
      if ('0' <= c && c <= '9') {
        for (j = i + 1; j < k; ++j) {
          c = OPENSSL_VERSION_TEXT[j];
          if (c == ' ')
            break;
        }
        break;
      }
    }
    READONLY_PROPERTY(
        versions,
        "openssl",
        OneByteString(env->isolate(), &OPENSSL_VERSION_TEXT[i], j - i));
  }
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
  READONLY_PROPERTY(release, "name", OneByteString(env->isolate(), "node"));

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
  Local<Array> arguments = Array::New(env->isolate(), argc);
  for (int i = 0; i < argc; ++i) {
    arguments->Set(i, String::NewFromUtf8(env->isolate(), argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "argv"), arguments);

  // process.execArgv
  Local<Array> exec_arguments = Array::New(env->isolate(), exec_argc);
  for (int i = 0; i < exec_argc; ++i) {
    exec_arguments->Set(i, String::NewFromUtf8(env->isolate(), exec_argv[i]));
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

  READONLY_PROPERTY(process, "pid", Integer::New(env->isolate(), getpid()));
  READONLY_PROPERTY(process, "features", GetFeatures(env));

  auto need_immediate_callback_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "_needImmediateCallback");
  CHECK(process->SetAccessor(env->context(), need_immediate_callback_string,
                             NeedImmediateCallbackGetter,
                             NeedImmediateCallbackSetter,
                             env->as_external()).FromJust());

  // -e, --eval
  if (eval_string) {
    READONLY_PROPERTY(process,
                      "_eval",
                      String::NewFromUtf8(env->isolate(), eval_string));
  }

  // -p, --print
  if (print_eval) {
    READONLY_PROPERTY(process, "_print_eval", True(env->isolate()));
  }

  // -c, --check
  if (syntax_check_only) {
    READONLY_PROPERTY(process, "_syntax_check_only", True(env->isolate()));
  }

  // -i, --interactive
  if (force_repl) {
    READONLY_PROPERTY(process, "_forceRepl", True(env->isolate()));
  }

  if (preload_module_count) {
    CHECK(preload_modules);
    Local<Array> array = Array::New(env->isolate());
    for (unsigned int i = 0; i < preload_module_count; ++i) {
      Local<String> module = String::NewFromUtf8(env->isolate(),
                                                 preload_modules[i]);
      array->Set(i, module);
    }
    READONLY_PROPERTY(process,
                      "_preload_modules",
                      array);

    delete[] preload_modules;
    preload_modules = nullptr;
    preload_module_count = 0;
  }

  // --no-deprecation
  if (no_deprecation) {
    READONLY_PROPERTY(process, "noDeprecation", True(env->isolate()));
  }

  if (no_process_warnings) {
    READONLY_PROPERTY(process, "noProcessWarnings", True(env->isolate()));
  }

  if (trace_warnings) {
    READONLY_PROPERTY(process, "traceProcessWarnings", True(env->isolate()));
  }

  // --throw-deprecation
  if (throw_deprecation) {
    READONLY_PROPERTY(process, "throwDeprecation", True(env->isolate()));
  }

#ifdef NODE_NO_BROWSER_GLOBALS
  // configure --no-browser-globals
  READONLY_PROPERTY(process, "_noBrowserGlobals", True(env->isolate()));
#endif  // NODE_NO_BROWSER_GLOBALS

  // --prof-process
  if (prof_process) {
    READONLY_PROPERTY(process, "profProcess", True(env->isolate()));
  }

  // --trace-deprecation
  if (trace_deprecation) {
    READONLY_PROPERTY(process, "traceDeprecation", True(env->isolate()));
  }

  // --debug-brk
  if (debug_options.wait_for_connect()) {
    READONLY_PROPERTY(process, "_debugWaitConnect", True(env->isolate()));
  }

  // --security-revert flags
#define V(code, _, __)                                                        \
  do {                                                                        \
    if (IsReverted(REVERT_ ## code)) {                                        \
      READONLY_PROPERTY(process, "REVERT_" #code, True(env->isolate()));      \
    }                                                                         \
  } while (0);
  REVERSIONS(V)
#undef V

  size_t exec_path_len = 2 * PATH_MAX;
  char* exec_path = new char[exec_path_len];
  Local<String> exec_path_value;
  if (uv_exepath(exec_path, &exec_path_len) == 0) {
    exec_path_value = String::NewFromUtf8(env->isolate(),
                                          exec_path,
                                          String::kNormalString,
                                          exec_path_len);
  } else {
    exec_path_value = String::NewFromUtf8(env->isolate(), argv[0]);
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "execPath"),
               exec_path_value);
  delete[] exec_path;

  auto debug_port_string = FIXED_ONE_BYTE_STRING(env->isolate(), "debugPort");
  CHECK(process->SetAccessor(env->context(),
                             debug_port_string,
                             DebugPortGetter,
                             DebugPortSetter,
                             env->as_external()).FromJust());

  // define various internal methods
  env->SetMethod(process,
                 "_startProfilerIdleNotifier",
                 StartProfilerIdleNotifier);
  env->SetMethod(process,
                 "_stopProfilerIdleNotifier",
                 StopProfilerIdleNotifier);
  env->SetMethod(process, "_getActiveRequests", GetActiveRequests);
  env->SetMethod(process, "_getActiveHandles", GetActiveHandles);
  env->SetMethod(process, "reallyExit", Exit);
  env->SetMethod(process, "abort", Abort);
  env->SetMethod(process, "chdir", Chdir);
  env->SetMethod(process, "cwd", Cwd);

  env->SetMethod(process, "umask", Umask);

#if defined(__POSIX__) && !defined(__ANDROID__)
  env->SetMethod(process, "getuid", GetUid);
  env->SetMethod(process, "geteuid", GetEUid);
  env->SetMethod(process, "setuid", SetUid);
  env->SetMethod(process, "seteuid", SetEUid);

  env->SetMethod(process, "setgid", SetGid);
  env->SetMethod(process, "setegid", SetEGid);
  env->SetMethod(process, "getgid", GetGid);
  env->SetMethod(process, "getegid", GetEGid);

  env->SetMethod(process, "getgroups", GetGroups);
  env->SetMethod(process, "setgroups", SetGroups);
  env->SetMethod(process, "initgroups", InitGroups);
#endif  // __POSIX__ && !defined(__ANDROID__)

  env->SetMethod(process, "_kill", Kill);

  env->SetMethod(process, "_debugProcess", DebugProcess);
  env->SetMethod(process, "_debugPause", DebugPause);
  env->SetMethod(process, "_debugEnd", DebugEnd);

  env->SetMethod(process, "hrtime", Hrtime);

  env->SetMethod(process, "cpuUsage", CPUUsage);

  env->SetMethod(process, "dlopen", DLOpen);

  env->SetMethod(process, "uptime", Uptime);
  env->SetMethod(process, "memoryUsage", MemoryUsage);

  env->SetMethod(process, "binding", Binding);
  env->SetMethod(process, "_linkedBinding", LinkedBinding);

  env->SetMethod(process, "_setupProcessObject", SetupProcessObject);
  env->SetMethod(process, "_setupNextTick", SetupNextTick);
  env->SetMethod(process, "_setupPromises", SetupPromises);
  env->SetMethod(process, "_setupDomainUse", SetupDomainUse);

  // pre-set _events object for faster emit checks
  Local<Object> events_obj = Object::New(env->isolate());
  CHECK(events_obj->SetPrototype(env->context(),
                                 Null(env->isolate())).FromJust());
  process->Set(env->events_string(), events_obj);
}


#undef READONLY_PROPERTY


void SignalExit(int signo) {
  uv_tty_reset_mode();
  if (trace_enabled) {
    v8_platform.StopTracingAgent();
  }
#ifdef __FreeBSD__
  // FreeBSD has a nasty bug, see RegisterSignalHandler for details
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(signo, &sa, nullptr), 0);
#endif
  raise(signo);
}


// Most of the time, it's best to use `console.error` to write
// to the process.stderr stream.  However, in some cases, such as
// when debugging the stream.Writable class or the process.nextTick
// function, it is useful to bypass JavaScript entirely.
static void RawDebug(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "must be called with a single string");
  node::Utf8Value message(args.GetIsolate(), args[0]);
  PrintErrorString("%s\n", *message);
  fflush(stderr);
}


void LoadEnvironment(Environment* env) {
  HandleScope handle_scope(env->isolate());

  TryCatch try_catch(env->isolate());

  // Disable verbose mode to stop FatalException() handler from trying
  // to handle the exception. Errors this early in the start-up phase
  // are not safe to ignore.
  try_catch.SetVerbose(false);

  // Execute the lib/internal/bootstrap_node.js file which was included as a
  // static C string in node_natives.h by node_js2c.
  // 'internal_bootstrap_node_native' is the string containing that source code.
  Local<String> script_name = FIXED_ONE_BYTE_STRING(env->isolate(),
                                                    "bootstrap_node.js");
  Local<Value> f_value = ExecuteString(env, MainSource(env), script_name);
  if (try_catch.HasCaught())  {
    ReportException(env, try_catch);
    exit(10);
  }
  // The bootstrap_node.js file returns a function 'f'
  CHECK(f_value->IsFunction());
  Local<Function> f = Local<Function>::Cast(f_value);

  // Add a reference to the global object
  Local<Object> global = env->context()->Global();

#if defined HAVE_DTRACE || defined HAVE_ETW
  InitDTrace(env, global);
#endif

#if defined HAVE_LTTNG
  InitLTTNG(env, global);
#endif

#if defined HAVE_PERFCTR
  InitPerfCounters(env, global);
#endif

  // Enable handling of uncaught exceptions
  // (FatalException(), break on uncaught exception in debugger)
  //
  // This is not strictly necessary since it's almost impossible
  // to attach the debugger fast enought to break on exception
  // thrown during process startup.
  try_catch.SetVerbose(true);

  env->SetMethod(env->process_object(), "_rawDebug", RawDebug);

  // Expose the global object as a property on itself
  // (Allows you to set stuff on `global` from anywhere in JavaScript.)
  global->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "global"), global);

  // Now we call 'f' with the 'process' variable that we've built up with
  // all our bindings. Inside bootstrap_node.js and internal/process we'll
  // take care of assigning things to their places.

  // We start the process this way in order to be more modular. Developers
  // who do not like how bootstrap_node.js sets up the module system but do
  // like Node's I/O bindings may want to replace 'f' with their own function.
  Local<Value> arg = env->process_object();
  f->Call(Null(env->isolate()), 1, &arg);
}

static void PrintHelp() {
  // XXX: If you add an option here, please also add it to doc/node.1 and
  // doc/api/cli.md
  printf("Usage: node [options] [ -e script | script.js ] [arguments]\n"
         "       node debug script.js [arguments]\n"
         "\n"
         "Options:\n"
         "  -v, --version              print Node.js version\n"
         "  -e, --eval script          evaluate script\n"
         "  -p, --print                evaluate script and print result\n"
         "  -c, --check                syntax check script without executing\n"
         "  -i, --interactive          always enter the REPL even if stdin\n"
         "                             does not appear to be a terminal\n"
         "  -r, --require              module to preload (option can be "
         "repeated)\n"
#if HAVE_INSPECTOR
         "  --inspect[=host:port]      activate inspector on host:port\n"
         "                             (default: 127.0.0.1:9229)\n"
         "  --inspect-brk[=host:port]  activate inspector on host:port\n"
         "                             and break at start of user script\n"
#endif
         "  --no-deprecation           silence deprecation warnings\n"
         "  --trace-deprecation        show stack traces on deprecations\n"
         "  --throw-deprecation        throw an exception on deprecations\n"
         "  --no-warnings              silence all process warnings\n"
         "  --trace-warnings           show stack traces on process warnings\n"
         "  --redirect-warnings=path\n"
         "                             write warnings to path instead of\n"
         "                             stderr\n"
         "  --trace-sync-io            show stack trace when use of sync IO\n"
         "                             is detected after the first tick\n"
         "  --trace-events-enabled     track trace events\n"
         "  --trace-event-categories   comma separated list of trace event\n"
         "                             categories to record\n"
         "  --track-heap-objects       track heap object allocations for heap "
         "snapshots\n"
         "  --prof-process             process v8 profiler output generated\n"
         "                             using --prof\n"
         "  --zero-fill-buffers        automatically zero-fill all newly "
         "allocated\n"
         "                             Buffer and SlowBuffer instances\n"
         "  --v8-options               print v8 command line options\n"
         "  --v8-pool-size=num         set v8's thread pool size\n"
#if HAVE_OPENSSL
         "  --tls-cipher-list=val      use an alternative default TLS cipher "
         "list\n"
         "  --use-bundled-ca           use bundled CA store"
#if !defined(NODE_OPENSSL_CERT_STORE)
         " (default)"
#endif
         "\n"
         "  --use-openssl-ca           use OpenSSL's default CA store"
#if defined(NODE_OPENSSL_CERT_STORE)
         " (default)"
#endif
         "\n"
#if NODE_FIPS_MODE
         "  --enable-fips              enable FIPS crypto at startup\n"
         "  --force-fips               force FIPS crypto (cannot be disabled)\n"
#endif  /* NODE_FIPS_MODE */
         "  --openssl-config=file      load OpenSSL configuration from the\n"
         "                             specified file (overrides\n"
         "                             OPENSSL_CONF)\n"
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
         "  --icu-data-dir=dir         set ICU data load path to dir\n"
         "                             (overrides NODE_ICU_DATA)\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                             note: linked-in ICU data is present\n"
#endif
         "  --preserve-symlinks        preserve symbolic links when resolving\n"
         "                             and caching modules\n"
#endif
         "\n"
         "Environment variables:\n"
         "NODE_DEBUG                   ','-separated list of core modules\n"
         "                             that should print debug information\n"
         "NODE_DISABLE_COLORS          set to 1 to disable colors in the REPL\n"
         "NODE_EXTRA_CA_CERTS          path to additional CA certificates\n"
         "                             file\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         "NODE_ICU_DATA                data path for ICU (Intl object) data\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                             (will extend linked-in data)\n"
#endif
#endif
         "NODE_NO_WARNINGS             set to 1 to silence process warnings\n"
#ifdef _WIN32
         "NODE_PATH                    ';'-separated list of directories\n"
#else
         "NODE_PATH                    ':'-separated list of directories\n"
#endif
         "                             prefixed to the module search path\n"
         "NODE_REPL_HISTORY            path to the persistent REPL history\n"
         "                             file\n"
         "NODE_REDIRECT_WARNINGS       write warnings to path instead of\n"
         "                             stderr\n"
         "OPENSSL_CONF                 load OpenSSL configuration from file\n"
         "\n"
         "Documentation can be found at https://nodejs.org/\n");
}


// Parse command line arguments.
//
// argv is modified in place. exec_argv and v8_argv are out arguments that
// ParseArgs() allocates memory for and stores a pointer to the output
// vector in.  The caller should free them with delete[].
//
// On exit:
//
//  * argv contains the arguments with node and V8 options filtered out.
//  * exec_argv contains both node and V8 options and nothing else.
//  * v8_argv contains argv[0] plus any V8 options
static void ParseArgs(int* argc,
                      const char** argv,
                      int* exec_argc,
                      const char*** exec_argv,
                      int* v8_argc,
                      const char*** v8_argv) {
  const unsigned int nargs = static_cast<unsigned int>(*argc);
  const char** new_exec_argv = new const char*[nargs];
  const char** new_v8_argv = new const char*[nargs];
  const char** new_argv = new const char*[nargs];
  const char** local_preload_modules = new const char*[nargs];

  for (unsigned int i = 0; i < nargs; ++i) {
    new_exec_argv[i] = nullptr;
    new_v8_argv[i] = nullptr;
    new_argv[i] = nullptr;
    local_preload_modules[i] = nullptr;
  }

  // exec_argv starts with the first option, the other two start with argv[0].
  unsigned int new_exec_argc = 0;
  unsigned int new_v8_argc = 1;
  unsigned int new_argc = 1;
  new_v8_argv[0] = argv[0];
  new_argv[0] = argv[0];

  unsigned int index = 1;
  bool short_circuit = false;
  while (index < nargs && argv[index][0] == '-' && !short_circuit) {
    const char* const arg = argv[index];
    unsigned int args_consumed = 1;

    if (debug_options.ParseOption(arg)) {
      // Done, consumed by DebugOptions::ParseOption().
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0 ||
               strcmp(arg, "-e") == 0 ||
               strcmp(arg, "--print") == 0 ||
               strcmp(arg, "-pe") == 0 ||
               strcmp(arg, "-p") == 0) {
      bool is_eval = strchr(arg, 'e') != nullptr;
      bool is_print = strchr(arg, 'p') != nullptr;
      print_eval = print_eval || is_print;
      // --eval, -e and -pe always require an argument.
      if (is_eval == true) {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (eval_string == nullptr) {
          fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
          exit(9);
        }
      } else if ((index + 1 < nargs) &&
                 argv[index + 1] != nullptr &&
                 argv[index + 1][0] != '-') {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (strncmp(eval_string, "\\-", 2) == 0) {
          // Starts with "\\-": escaped expression, drop the backslash.
          eval_string += 1;
        }
      }
    } else if (strcmp(arg, "--require") == 0 ||
               strcmp(arg, "-r") == 0) {
      const char* module = argv[index + 1];
      if (module == nullptr) {
        fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
        exit(9);
      }
      args_consumed += 1;
      local_preload_modules[preload_module_count++] = module;
    } else if (strcmp(arg, "--check") == 0 || strcmp(arg, "-c") == 0) {
      syntax_check_only = true;
    } else if (strcmp(arg, "--interactive") == 0 || strcmp(arg, "-i") == 0) {
      force_repl = true;
    } else if (strcmp(arg, "--no-deprecation") == 0) {
      no_deprecation = true;
    } else if (strcmp(arg, "--no-warnings") == 0) {
      no_process_warnings = true;
    } else if (strcmp(arg, "--trace-warnings") == 0) {
      trace_warnings = true;
    } else if (strncmp(arg, "--redirect-warnings=", 20) == 0) {
      config_warning_file = arg + 20;
    } else if (strcmp(arg, "--trace-deprecation") == 0) {
      trace_deprecation = true;
    } else if (strcmp(arg, "--trace-sync-io") == 0) {
      trace_sync_io = true;
    } else if (strcmp(arg, "--trace-events-enabled") == 0) {
      trace_enabled = true;
    } else if (strcmp(arg, "--trace-event-categories") == 0) {
      const char* categories = argv[index + 1];
      if (categories == nullptr) {
        fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
        exit(9);
      }
      args_consumed += 1;
      trace_enabled_categories = categories;
    } else if (strcmp(arg, "--track-heap-objects") == 0) {
      track_heap_objects = true;
    } else if (strcmp(arg, "--throw-deprecation") == 0) {
      throw_deprecation = true;
    } else if (strncmp(arg, "--security-revert=", 18) == 0) {
      const char* cve = arg + 18;
      Revert(cve);
    } else if (strcmp(arg, "--preserve-symlinks") == 0) {
      config_preserve_symlinks = true;
    } else if (strcmp(arg, "--prof-process") == 0) {
      prof_process = true;
      short_circuit = true;
    } else if (strcmp(arg, "--zero-fill-buffers") == 0) {
      zero_fill_all_buffers = true;
    } else if (strcmp(arg, "--v8-options") == 0) {
      new_v8_argv[new_v8_argc] = "--help";
      new_v8_argc += 1;
    } else if (strncmp(arg, "--v8-pool-size=", 15) == 0) {
      v8_thread_pool_size = atoi(arg + 15);
#if HAVE_OPENSSL
    } else if (strncmp(arg, "--tls-cipher-list=", 18) == 0) {
      default_cipher_list = arg + 18;
    } else if (strncmp(arg, "--use-openssl-ca", 16) == 0) {
      ssl_openssl_cert_store = true;
    } else if (strncmp(arg, "--use-bundled-ca", 16) == 0) {
      ssl_openssl_cert_store = false;
#if NODE_FIPS_MODE
    } else if (strcmp(arg, "--enable-fips") == 0) {
      enable_fips_crypto = true;
    } else if (strcmp(arg, "--force-fips") == 0) {
      force_fips_crypto = true;
#endif /* NODE_FIPS_MODE */
    } else if (strncmp(arg, "--openssl-config=", 17) == 0) {
      openssl_config.assign(arg + 17);
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
    } else if (strncmp(arg, "--icu-data-dir=", 15) == 0) {
      icu_data_dir.assign(arg + 15);
#endif
    } else if (strcmp(arg, "--expose-internals") == 0 ||
               strcmp(arg, "--expose_internals") == 0) {
      // consumed in js
    } else if (strcmp(arg, "--") == 0) {
      index += 1;
      break;
    } else {
      // V8 option.  Pass through as-is.
      new_v8_argv[new_v8_argc] = arg;
      new_v8_argc += 1;
    }

    memcpy(new_exec_argv + new_exec_argc,
           argv + index,
           args_consumed * sizeof(*argv));

    new_exec_argc += args_consumed;
    index += args_consumed;
  }

  // Copy remaining arguments.
  const unsigned int args_left = nargs - index;
  memcpy(new_argv + new_argc, argv + index, args_left * sizeof(*argv));
  new_argc += args_left;

  *exec_argc = new_exec_argc;
  *exec_argv = new_exec_argv;
  *v8_argc = new_v8_argc;
  *v8_argv = new_v8_argv;

  // Copy new_argv over argv and update argc.
  memcpy(argv, new_argv, new_argc * sizeof(*argv));
  delete[] new_argv;
  *argc = static_cast<int>(new_argc);

  // Copy the preload_modules from the local array to an appropriately sized
  // global array.
  if (preload_module_count > 0) {
    CHECK(!preload_modules);
    preload_modules = new const char*[preload_module_count];
    memcpy(preload_modules, local_preload_modules,
           preload_module_count * sizeof(*preload_modules));
  }
  delete[] local_preload_modules;
}


// Called from V8 Debug Agent TCP thread.
static void DispatchMessagesDebugAgentCallback(Environment* env) {
  // TODO(indutny): move async handle to environment
  uv_async_send(&dispatch_debug_messages_async);
}


static void StartDebug(Environment* env, const char* path,
                       DebugOptions debug_options) {
  CHECK(!debugger_running);
#if HAVE_INSPECTOR
  if (debug_options.inspector_enabled())
    debugger_running = v8_platform.StartInspector(env, path, debug_options);
#endif  // HAVE_INSPECTOR
  if (debug_options.debugger_enabled()) {
    env->debugger_agent()->set_dispatch_handler(
          DispatchMessagesDebugAgentCallback);
    debugger_running = env->debugger_agent()->Start(debug_options);
    if (debugger_running == false) {
      fprintf(stderr, "Starting debugger on %s:%d failed\n",
              debug_options.host_name().c_str(), debug_options.port());
      fflush(stderr);
    }
  }
}


// Called from the main thread.
static void EnableDebug(Environment* env) {
  CHECK(debugger_running);

  if (!debug_options.debugger_enabled()) {
    return;
  }

  // Send message to enable debug in workers
  HandleScope handle_scope(env->isolate());

  Local<Object> message = Object::New(env->isolate());
  message->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "cmd"),
               FIXED_ONE_BYTE_STRING(env->isolate(), "NODE_DEBUG_ENABLED"));
  Local<Value> argv[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "internalMessage"),
    message
  };
  MakeCallback(env, env->process_object(), "emit", arraysize(argv), argv);

  // Enabled debugger, possibly making it wait on a semaphore
  env->debugger_agent()->Enable();
}


// Called from an arbitrary thread.
static void TryStartDebugger() {
  Mutex::ScopedLock scoped_lock(node_isolate_mutex);
  if (auto isolate = node_isolate) {
    v8::Debug::DebugBreak(isolate);
    uv_async_send(&dispatch_debug_messages_async);
  }
}


// Called from the main thread.
static void DispatchDebugMessagesAsyncCallback(uv_async_t* handle) {
  Mutex::ScopedLock scoped_lock(node_isolate_mutex);
  if (auto isolate = node_isolate) {
    if (debugger_running == false) {
      fprintf(stderr, "Starting debugger agent.\n");

      HandleScope scope(isolate);
      Environment* env = Environment::GetCurrent(isolate);
      Context::Scope context_scope(env->context());
      debug_options.EnableDebugAgent(DebugAgentType::kDebugger);
      StartDebug(env, nullptr, debug_options);
      EnableDebug(env);
    }

    Isolate::Scope isolate_scope(isolate);
    v8::Debug::ProcessDebugMessages(isolate);
  }
}


#ifdef __POSIX__
static void EnableDebugSignalHandler(int signo) {
  uv_sem_post(&debug_semaphore);
}


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

  pid_t pid;
  int r;

  pid = args[0]->IntegerValue();
  r = kill(pid, SIGUSR1);
  if (r != 0) {
    return env->ThrowErrnoException(errno, "kill");
  }
}


inline void* DebugSignalThreadMain(void* unused) {
  for (;;) {
    uv_sem_wait(&debug_semaphore);
    TryStartDebugger();
  }
  return nullptr;
}


static int RegisterDebugSignalHandler() {
  // Start a watchdog thread for calling v8::Debug::DebugBreak() because
  // it's not safe to call directly from the signal handler, it can
  // deadlock with the thread it interrupts.
  CHECK_EQ(0, uv_sem_init(&debug_semaphore, 0));
  pthread_attr_t attr;
  CHECK_EQ(0, pthread_attr_init(&attr));
  // Don't shrink the thread's stack on FreeBSD.  Said platform decided to
  // follow the pthreads specification to the letter rather than in spirit:
  // https://lists.freebsd.org/pipermail/freebsd-current/2014-March/048885.html
#ifndef __FreeBSD__
  CHECK_EQ(0, pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN));
#endif  // __FreeBSD__
  CHECK_EQ(0, pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  sigset_t sigmask;
  sigfillset(&sigmask);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &sigmask));
  pthread_t thread;
  const int err =
      pthread_create(&thread, &attr, DebugSignalThreadMain, nullptr);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  CHECK_EQ(0, pthread_attr_destroy(&attr));
  if (err != 0) {
    fprintf(stderr, "node[%d]: pthread_create: %s\n", getpid(), strerror(err));
    fflush(stderr);
    // Leave SIGUSR1 blocked.  We don't install a signal handler,
    // receiving the signal would terminate the process.
    return -err;
  }
  RegisterSignalHandler(SIGUSR1, EnableDebugSignalHandler);
  // Unblock SIGUSR1.  A pending SIGUSR1 signal will now be delivered.
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr));
  return 0;
}
#endif  // __POSIX__


#ifdef _WIN32
DWORD WINAPI EnableDebugThreadProc(void* arg) {
  TryStartDebugger();
  return 0;
}


static int GetDebugSignalHandlerMappingName(DWORD pid, wchar_t* buf,
    size_t buf_len) {
  return _snwprintf(buf, buf_len, L"node-debug-handler-%u", pid);
}


static int RegisterDebugSignalHandler() {
  wchar_t mapping_name[32];
  HANDLE mapping_handle;
  DWORD pid;
  LPTHREAD_START_ROUTINE* handler;

  pid = GetCurrentProcessId();

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    return -1;
  }

  mapping_handle = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                      nullptr,
                                      PAGE_READWRITE,
                                      0,
                                      sizeof *handler,
                                      mapping_name);
  if (mapping_handle == nullptr) {
    return -1;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping_handle,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    sizeof *handler));
  if (handler == nullptr) {
    CloseHandle(mapping_handle);
    return -1;
  }

  *handler = EnableDebugThreadProc;

  UnmapViewOfFile(static_cast<void*>(handler));

  return 0;
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  DWORD pid;
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE mapping = nullptr;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = nullptr;

  if (args.Length() != 1) {
    env->ThrowError("Invalid number of arguments.");
    goto out;
  }

  pid = (DWORD) args[0]->IntegerValue();

  process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                            PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                            PROCESS_VM_READ,
                        FALSE,
                        pid);
  if (process == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "OpenProcess"));
    goto out;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    env->ThrowErrnoException(errno, "sprintf");
    goto out;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                             GetLastError(),
                                             "OpenFileMappingW"));
    goto out;
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
    goto out;
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
    goto out;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                                 GetLastError(),
                                                 "WaitForSingleObject"));
    goto out;
  }

 out:
  if (process != nullptr)
    CloseHandle(process);
  if (thread != nullptr)
    CloseHandle(thread);
  if (handler != nullptr)
    UnmapViewOfFile(handler);
  if (mapping != nullptr)
    CloseHandle(mapping);
}
#endif  // _WIN32


static void DebugPause(const FunctionCallbackInfo<Value>& args) {
  v8::Debug::DebugBreak(args.GetIsolate());
}


static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
  if (debugger_running) {
    Environment* env = Environment::GetCurrent(args);
#if HAVE_INSPECTOR
    if (debug_options.inspector_enabled()) {
      env->inspector_agent()->Stop();
    } else {
#endif
      env->debugger_agent()->Stop();
#if HAVE_INSPECTOR
    }
#endif
    debugger_running = false;
  }
}


inline void PlatformInit() {
#ifdef __POSIX__
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  const int err = pthread_sigmask(SIG_SETMASK, &sigmask, nullptr);

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

  CHECK_EQ(err, 0);

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
}


void Init(int* argc,
          const char** argv,
          int* exec_argc,
          const char*** exec_argv) {
  // Initialize prog_start_time to get relative uptime.
  prog_start_time = static_cast<double>(uv_now(uv_default_loop()));

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // init async debug messages dispatching
  // Main thread uses uv_default_loop
  CHECK_EQ(0, uv_async_init(uv_default_loop(),
                            &dispatch_debug_messages_async,
                            DispatchDebugMessagesAsyncCallback));
  uv_unref(reinterpret_cast<uv_handle_t*>(&dispatch_debug_messages_async));

#if defined(NODE_HAVE_I18N_SUPPORT)
  // Set the ICU casing flag early
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  const char icu_case_mapping[] = "--icu_case_mapping";
  V8::SetFlagsFromString(icu_case_mapping, sizeof(icu_case_mapping) - 1);
#endif

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

  // Allow for environment set preserving symlinks.
  {
    std::string text;
    config_preserve_symlinks =
        SafeGetenv("NODE_PRESERVE_SYMLINKS", &text) && text[0] == '1';
  }

  if (config_warning_file.empty())
    SafeGetenv("NODE_REDIRECT_WARNINGS", &config_warning_file);

  if (openssl_config.empty())
    SafeGetenv("OPENSSL_CONF", &openssl_config);

  // Parse a few arguments which are specific to Node.
  int v8_argc;
  const char** v8_argv;
  ParseArgs(argc, argv, exec_argc, exec_argv, &v8_argc, &v8_argv);

  // TODO(bnoordhuis) Intercept --prof arguments and start the CPU profiler
  // manually?  That would give us a little more control over its runtime
  // behavior but it could also interfere with the user's intentions in ways
  // we fail to anticipate.  Dillema.
  for (int i = 1; i < v8_argc; ++i) {
    if (strncmp(v8_argv[i], "--prof", sizeof("--prof") - 1) == 0) {
      v8_is_profiling = true;
      break;
    }
  }

#ifdef __POSIX__
  // Block SIGPROF signals when sleeping in epoll_wait/kevent/etc.  Avoids the
  // performance penalty of frequent EINTR wakeups when the profiler is running.
  // Only do this for v8.log profiling, as it breaks v8::CpuProfiler users.
  if (v8_is_profiling) {
    uv_loop_configure(uv_default_loop(), UV_LOOP_BLOCK_SIGNAL, SIGPROF);
  }
#endif

#if defined(NODE_HAVE_I18N_SUPPORT)
  // If the parameter isn't given, use the env variable.
  if (icu_data_dir.empty())
    SafeGetenv("NODE_ICU_DATA", &icu_data_dir);
  // Initialize ICU.
  // If icu_data_dir is empty here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(icu_data_dir)) {
    FatalError(nullptr, "Could not initialize ICU "
                     "(check NODE_ICU_DATA or --icu-data-dir parameters)");
  }
#endif
  // The const_cast doesn't violate conceptual const-ness.  V8 doesn't modify
  // the argv array or the elements it points to.
  if (v8_argc > 1)
    V8::SetFlagsFromCommandLine(&v8_argc, const_cast<char**>(v8_argv), true);

  // Anything that's still in v8_argv is not a V8 or a node option.
  for (int i = 1; i < v8_argc; i++) {
    fprintf(stderr, "%s: bad option: %s\n", argv[0], v8_argv[i]);
  }
  delete[] v8_argv;
  v8_argv = nullptr;

  if (v8_argc > 1) {
    exit(9);
  }

  // Unconditionally force typed arrays to allocate outside the v8 heap. This
  // is to prevent memory pointers from being moved around that are returned by
  // Buffer::Data().
  const char no_typed_array_heap[] = "--typed_array_max_size_in_heap=0";
  V8::SetFlagsFromString(no_typed_array_heap, sizeof(no_typed_array_heap) - 1);

  if (!debug_options.debugger_enabled() && !debug_options.inspector_enabled()) {
    RegisterDebugSignalHandler();
  }

  // We should set node_is_initialized here instead of in node::Start,
  // otherwise embedders using node::Init to initialize everything will not be
  // able to set it and native modules will not load for them.
  node_is_initialized = true;
}


struct AtExitCallback {
  AtExitCallback* next_;
  void (*cb_)(void* arg);
  void* arg_;
};

static AtExitCallback* at_exit_functions_;


// TODO(bnoordhuis) Turn into per-context event.
void RunAtExit(Environment* env) {
  AtExitCallback* p = at_exit_functions_;
  at_exit_functions_ = nullptr;

  while (p) {
    AtExitCallback* q = p->next_;
    p->cb_(p->arg_);
    delete p;
    p = q;
  }
}


void AtExit(void (*cb)(void* arg), void* arg) {
  AtExitCallback* p = new AtExitCallback;
  p->cb_ = cb;
  p->arg_ = arg;
  p->next_ = at_exit_functions_;
  at_exit_functions_ = p;
}


void EmitBeforeExit(Environment* env) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  Local<String> exit_code = FIXED_ONE_BYTE_STRING(env->isolate(), "exitCode");
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "beforeExit"),
    process_object->Get(exit_code)->ToInteger(env->isolate())
  };
  MakeCallback(env, process_object, "emit", arraysize(args), args);
}


int EmitExit(Environment* env) {
  // process.emit('exit')
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  process_object->Set(env->exiting_string(), True(env->isolate()));

  Local<String> exitCode = env->exit_code_string();
  int code = process_object->Get(exitCode)->Int32Value();

  Local<Value> args[] = {
    env->exit_string(),
    Integer::New(env->isolate(), code)
  };

  MakeCallback(env, process_object, "emit", arraysize(args), args);

  // Reload exit code, it may be changed by `emit('exit')`
  return process_object->Get(exitCode)->Int32Value();
}


IsolateData* CreateIsolateData(Isolate* isolate, uv_loop_t* loop) {
  return new IsolateData(isolate, loop);
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
  auto env = new Environment(isolate_data, context);
  env->Start(argc, argv, exec_argc, exec_argv, v8_is_profiling);
  return env;
}


void FreeEnvironment(Environment* env) {
  delete env;
}


inline int Start(Isolate* isolate, IsolateData* isolate_data,
                 int argc, const char* const* argv,
                 int exec_argc, const char* const* exec_argv) {
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);
  Environment env(isolate_data, context);
  env.Start(argc, argv, exec_argc, exec_argv, v8_is_profiling);

  bool debug_enabled =
      debug_options.debugger_enabled() || debug_options.inspector_enabled();

  // Start debug agent when argv has --debug
  if (debug_enabled) {
    const char* path = argc > 1 ? argv[1] : nullptr;
    StartDebug(&env, path, debug_options);
    if (!debugger_running)
      return 12;  // Signal internal error.
  }

  {
    Environment::AsyncCallbackScope callback_scope(&env);
    LoadEnvironment(&env);
  }

  env.set_trace_sync_io(trace_sync_io);

  // Enable debugger
  if (debug_enabled)
    EnableDebug(&env);

  {
    SealHandleScope seal(isolate);
    bool more;
    do {
      v8_platform.PumpMessageLoop(isolate);
      more = uv_run(env.event_loop(), UV_RUN_ONCE);

      if (more == false) {
        v8_platform.PumpMessageLoop(isolate);
        EmitBeforeExit(&env);

        // Emit `beforeExit` if the loop became alive either after emitting
        // event, or after running some callbacks.
        more = uv_loop_alive(env.event_loop());
        if (uv_run(env.event_loop(), UV_RUN_NOWAIT) != 0)
          more = true;
      }
    } while (more == true);
  }

  env.set_trace_sync_io(false);

  const int exit_code = EmitExit(&env);
  RunAtExit(&env);

  WaitForInspectorDisconnect(&env);
#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif

  return exit_code;
}

inline int Start(uv_loop_t* event_loop,
                 int argc, const char* const* argv,
                 int exec_argc, const char* const* exec_argv) {
  Isolate::CreateParams params;
  ArrayBufferAllocator allocator;
  params.array_buffer_allocator = &allocator;
#ifdef NODE_ENABLE_VTUNE_PROFILING
  params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif

  Isolate* const isolate = Isolate::New(params);
  if (isolate == nullptr)
    return 12;  // Signal internal error.

  isolate->AddMessageListener(OnMessage);
  isolate->SetAbortOnUncaughtExceptionCallback(ShouldAbortOnUncaughtException);
  isolate->SetAutorunMicrotasks(false);
  isolate->SetFatalErrorHandler(OnFatalError);

  if (track_heap_objects) {
    isolate->GetHeapProfiler()->StartTrackingHeapObjects(true);
  }

  {
    Mutex::ScopedLock scoped_lock(node_isolate_mutex);
    CHECK_EQ(node_isolate, nullptr);
    node_isolate = isolate;
  }

  int exit_code;
  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    IsolateData isolate_data(isolate, event_loop, allocator.zero_fill_field());
    exit_code = Start(isolate, &isolate_data, argc, argv, exec_argc, exec_argv);
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

  CHECK_GT(argc, 0);

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  // This needs to run *before* V8::Initialize().  The const_cast is not
  // optional, in case you're wondering.
  int exec_argc;
  const char** exec_argv;
  Init(&argc, const_cast<const char**>(argv), &exec_argc, &exec_argv);

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

  v8_platform.Initialize(v8_thread_pool_size);
  // Enable tracing when argv has --trace-events-enabled.
  if (trace_enabled) {
    fprintf(stderr, "Warning: Trace event is an experimental feature "
            "and could change at any time.\n");
    v8_platform.StartTracingAgent();
  }
  V8::Initialize();
  v8_initialized = true;
  const int exit_code =
      Start(uv_default_loop(), argc, argv, exec_argc, exec_argv);
  if (trace_enabled) {
    v8_platform.StopTracingAgent();
  }
  v8_initialized = false;
  V8::Dispose();

  v8_platform.Dispose();

  delete[] exec_argv;
  exec_argv = nullptr;

  return exit_code;
}


}  // namespace node

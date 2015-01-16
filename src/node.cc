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

#include "node.h"
#include "node_buffer.h"
#include "node_constants.h"
#include "node_file.h"
#include "node_http_parser.h"
#include "node_javascript.h"
#include "node_version.h"

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

#include "ares.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "req_wrap.h"
#include "string_bytes.h"
#include "util.h"
#include "uv.h"
#include "v8-debug.h"
#include "v8-profiler.h"
#include "zlib.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>  // PATH_MAX
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#include <process.h>
#define strcasecmp _stricmp
#define getpid _getpid
#define umask _umask
typedef int mode_t;
#else
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
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Message;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::TryCatch;
using v8::Uint32;
using v8::V8;
using v8::Value;
using v8::kExternalUint32Array;

static bool print_eval = false;
static bool force_repl = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static const char* eval_string = NULL;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static int debug_port = 5858;
static bool v8_is_profiling = false;
static bool node_is_initialized = false;
static node_module* modpending;
static node_module* modlist_builtin;
static node_module* modlist_linked;
static node_module* modlist_addon;

#if defined(NODE_HAVE_I18N_SUPPORT)
// Path to ICU data (for i18n / Intl)
static const char* icu_data_dir = NULL;
#endif

// used by C++ modules as well
bool no_deprecation = false;

// process-relative uptime base, initialized at start-up
static double prog_start_time;
static bool debugger_running;
static uv_async_t dispatch_debug_messages_async;

static Isolate* node_isolate = NULL;

int WRITE_UTF8_FLAGS = v8::String::HINT_MANY_WRITES_EXPECTED |
                       v8::String::NO_NULL_TERMINATION;

class ArrayBufferAllocator : public ArrayBuffer::Allocator {
 public:
  // Impose an upper limit to avoid out of memory errors that bring down
  // the process.
  static const size_t kMaxLength = 0x3fffffff;
  static ArrayBufferAllocator the_singleton;
  virtual ~ArrayBufferAllocator() {}
  virtual void* Allocate(size_t length);
  virtual void* AllocateUninitialized(size_t length);
  virtual void Free(void* data, size_t length);
 private:
  ArrayBufferAllocator() {}
  ArrayBufferAllocator(const ArrayBufferAllocator&);
  void operator=(const ArrayBufferAllocator&);
};

ArrayBufferAllocator ArrayBufferAllocator::the_singleton;


void* ArrayBufferAllocator::Allocate(size_t length) {
  if (length > kMaxLength)
    return NULL;
  char* data = new char[length];
  memset(data, 0, length);
  return data;
}


void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  if (length > kMaxLength)
    return NULL;
  return new char[length];
}


void ArrayBufferAllocator::Free(void* data, size_t length) {
  delete[] static_cast<char*>(data);
}


static void CheckImmediate(uv_check_t* handle) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MakeCallback(env, env->process_object(), env->immediate_callback_string());
}


static void IdleImmediateDummy(uv_idle_t* handle) {
  // Do nothing. Only for maintaining event loop.
  // TODO(bnoordhuis) Maybe make libuv accept NULL idle callbacks.
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
  ERRNO_CASE(ENOTEMPTY);
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
  SIGNO_CASE(SIGLOST);
#endif

#ifdef SIGPWR
# if SIGPWR != SIGLOST
  SIGNO_CASE(SIGPWR);
# endif
#endif

#ifdef SIGSYS
  SIGNO_CASE(SIGSYS);
#endif

  default: return "";
  }
}


// Convenience methods


void ThrowError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowError(errmsg);
}


void ThrowTypeError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowTypeError(errmsg);
}


void ThrowRangeError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowRangeError(errmsg);
}


void ThrowErrnoException(v8::Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* message,
                         const char* path) {
  Environment::GetCurrent(isolate)->ThrowErrnoException(errorno,
                                                        syscall,
                                                        message,
                                                        path);
}


void ThrowUVException(v8::Isolate* isolate,
                      int errorno,
                      const char* syscall,
                      const char* message,
                      const char* path) {
  Environment::GetCurrent(isolate)->ThrowErrnoException(errorno,
                                                        syscall,
                                                        message,
                                                        path);
}


Local<Value> ErrnoException(Isolate* isolate,
                            int errorno,
                            const char *syscall,
                            const char *msg,
                            const char *path) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Value> e;
  Local<String> estring = OneByteString(env->isolate(), errno_string(errorno));
  if (msg == NULL || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(env->isolate(), msg);

  Local<String> cons1 =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(env->isolate(), ", "));
  Local<String> cons2 = String::Concat(cons1, message);

  if (path) {
    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(env->isolate(), " '"));
    Local<String> cons4 =
        String::Concat(cons3, String::NewFromUtf8(env->isolate(), path));
    Local<String> cons5 =
        String::Concat(cons4, FIXED_ONE_BYTE_STRING(env->isolate(), "'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();
  obj->Set(env->errno_string(), Integer::New(env->isolate(), errorno));
  obj->Set(env->code_string(), estring);

  if (path != NULL) {
    obj->Set(env->path_string(), String::NewFromUtf8(env->isolate(), path));
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(env->isolate(), syscall));
  }

  return e;
}


// hack alert! copy of ErrnoException, tuned for uv errors
Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char *syscall,
                         const char *msg,
                         const char *path) {
  Environment* env = Environment::GetCurrent(isolate);

  if (!msg || !msg[0])
    msg = uv_strerror(errorno);

  Local<String> estring = OneByteString(env->isolate(), uv_err_name(errorno));
  Local<String> message = OneByteString(env->isolate(), msg);
  Local<String> cons1 =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(env->isolate(), ", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e;

  Local<String> path_str;

  if (path) {
#ifdef _WIN32
    if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
      path_str = String::Concat(FIXED_ONE_BYTE_STRING(env->isolate(), "\\\\"),
                                String::NewFromUtf8(env->isolate(), path + 8));
    } else if (strncmp(path, "\\\\?\\", 4) == 0) {
      path_str = String::NewFromUtf8(env->isolate(), path + 4);
    } else {
      path_str = String::NewFromUtf8(env->isolate(), path);
    }
#else
    path_str = String::NewFromUtf8(env->isolate(), path);
#endif

    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(env->isolate(), " '"));
    Local<String> cons4 =
        String::Concat(cons3, path_str);
    Local<String> cons5 =
        String::Concat(cons4, FIXED_ONE_BYTE_STRING(env->isolate(), "'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();
  // TODO(piscisaureus) errno should probably go
  obj->Set(env->errno_string(), Integer::New(env->isolate(), errorno));
  obj->Set(env->code_string(), estring);

  if (path != NULL) {
    obj->Set(env->path_string(), path_str);
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(env->isolate(), syscall));
  }

  return e;
}


#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char *winapi_strerror(const int errorno, bool* must_free) {
  char *errmsg = NULL;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, NULL);

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

  Local<Object> obj = e->ToObject();
  obj->Set(env->errno_string(), Integer::New(isolate, errorno));

  if (path != NULL) {
    obj->Set(env->path_string(), String::NewFromUtf8(isolate, path));
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(isolate, syscall));
  }

  if (must_free)
    LocalFree((HLOCAL)msg);

  return e;
}
#endif


void SetupDomainUse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());

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
    abort();
  }

  process_object->Set(env->tick_callback_string(), tick_callback_function);
  env->set_tick_callback_function(tick_callback_function);

  assert(args[0]->IsArray());
  assert(args[1]->IsObject());

  env->set_domain_array(args[0].As<Array>());

  Local<Object> domain_flag_obj = args[1].As<Object>();
  Environment::DomainFlag* domain_flag = env->domain_flag();
  domain_flag_obj->SetIndexedPropertiesToExternalArrayData(
      domain_flag->fields(),
      kExternalUint32Array,
      domain_flag->fields_count());

  // Do a little housekeeping.
  env->process_object()->Delete(
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupDomainUse"));
}

void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->RunMicrotasks();
}


void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  assert(args[0]->IsObject());
  assert(args[1]->IsFunction());
  assert(args[2]->IsObject());

  // Values use to cross communicate with processNextTick.
  Local<Object> tick_info_obj = args[0].As<Object>();
  tick_info_obj->SetIndexedPropertiesToExternalArrayData(
      env->tick_info()->fields(),
      kExternalUint32Array,
      env->tick_info()->fields_count());

  env->set_tick_callback_function(args[1].As<Function>());

  NODE_SET_METHOD(args[2].As<Object>(), "runMicrotasks", RunMicrotasks);

  // Do a little housekeeping.
  env->process_object()->Delete(
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupNextTick"));
}


Handle<Value> MakeCallback(Environment* env,
                           Handle<Value> recv,
                           const Handle<Function> callback,
                           int argc,
                           Handle<Value> argv[]) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  CHECK(env->context() == env->isolate()->GetCurrentContext());

  Local<Object> process = env->process_object();
  Local<Object> object, domain;
  bool has_async_queue = false;
  bool has_domain = false;

  if (recv->IsObject()) {
    object = recv.As<Object>();
    Local<Value> async_queue_v = object->Get(env->async_queue_string());
    if (async_queue_v->IsObject())
      has_async_queue = true;
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

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  if (has_domain) {
    Local<Value> enter_v = domain->Get(env->enter_string());
    if (enter_v->IsFunction()) {
      enter_v.As<Function>()->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env->isolate());
    }
  }

  if (has_async_queue) {
    try_catch.SetVerbose(false);
    env->async_hooks_pre_function()->Call(object, 0, NULL);
    if (try_catch.HasCaught())
      FatalError("node:;MakeCallback", "pre hook threw");
    try_catch.SetVerbose(true);
  }

  Local<Value> ret = callback->Call(recv, argc, argv);

  if (has_async_queue) {
    try_catch.SetVerbose(false);
    env->async_hooks_post_function()->Call(object, 0, NULL);
    if (try_catch.HasCaught())
      FatalError("node::MakeCallback", "post hook threw");
    try_catch.SetVerbose(true);
  }

  if (has_domain) {
    Local<Value> exit_v = domain->Get(env->exit_string());
    if (exit_v->IsFunction()) {
      exit_v.As<Function>()->Call(domain, 0, NULL);
      if (try_catch.HasCaught())
        return Undefined(env->isolate());
    }
  }

  if (try_catch.HasCaught()) {
    return Undefined(env->isolate());
  }

  Environment::TickInfo* tick_info = env->tick_info();

  if (tick_info->in_tick()) {
    return ret;
  }

  if (tick_info->length() == 0) {
    env->isolate()->RunMicrotasks();
  }

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  tick_info->set_in_tick(true);

  // process nextTicks after call
  env->tick_callback_function()->Call(process, 0, NULL);

  tick_info->set_in_tick(false);

  if (try_catch.HasCaught()) {
    tick_info->set_last_threw(true);
    return Undefined(env->isolate());
  }

  return ret;
}


// Internal only.
Handle<Value> MakeCallback(Environment* env,
                           Handle<Object> recv,
                           uint32_t index,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Value> cb_v = recv->Get(index);
  CHECK(cb_v->IsFunction());
  return MakeCallback(env, recv.As<Value>(), cb_v.As<Function>(), argc, argv);
}


Handle<Value> MakeCallback(Environment* env,
                           Handle<Object> recv,
                           Handle<String> symbol,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Value> cb_v = recv->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(env, recv.As<Value>(), cb_v.As<Function>(), argc, argv);
}


Handle<Value> MakeCallback(Environment* env,
                           Handle<Object> recv,
                           const char* method,
                           int argc,
                           Handle<Value> argv[]) {
  Local<String> method_string = OneByteString(env->isolate(), method);
  return MakeCallback(env, recv, method_string, argc, argv);
}


Handle<Value> MakeCallback(Isolate* isolate,
                           Handle<Object> recv,
                           const char* method,
                           int argc,
                           Handle<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = recv->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  return handle_scope.Escape(
      Local<Value>::New(isolate, MakeCallback(env, recv, method, argc, argv)));
}


Handle<Value> MakeCallback(Isolate* isolate,
                           Handle<Object> recv,
                           Handle<String> symbol,
                           int argc,
                           Handle<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = recv->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  return handle_scope.Escape(
      Local<Value>::New(isolate, MakeCallback(env, recv, symbol, argc, argv)));
}


Handle<Value> MakeCallback(Isolate* isolate,
                           Handle<Object> recv,
                           Handle<Function> callback,
                           int argc,
                           Handle<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = recv->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  return handle_scope.Escape(Local<Value>::New(
        isolate,
        MakeCallback(env, recv.As<Value>(), callback, argc, argv)));
}


enum encoding ParseEncoding(Isolate* isolate,
                            Handle<Value> encoding_v,
                            enum encoding _default) {
  HandleScope scope(isolate);

  if (!encoding_v->IsString())
    return _default;

  node::Utf8Value encoding(encoding_v);

  if (strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "utf-8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else if (strcasecmp(*encoding, "base64") == 0) {
    return BASE64;
  } else if (strcasecmp(*encoding, "ucs2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "ucs-2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "utf16le") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "utf-16le") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  } else if (strcasecmp(*encoding, "buffer") == 0) {
    return BUFFER;
  } else if (strcasecmp(*encoding, "hex") == 0) {
    return HEX;
  } else if (strcasecmp(*encoding, "raw") == 0) {
    if (!no_deprecation) {
      fprintf(stderr, "'raw' (array of integers) has been removed. "
                      "Use 'binary'.\n");
    }
    return BINARY;
  } else if (strcasecmp(*encoding, "raws") == 0) {
    if (!no_deprecation) {
      fprintf(stderr, "'raws' encoding has been renamed to 'binary'. "
                      "Please update your code.\n");
    }
    return BINARY;
  } else {
    return _default;
  }
}

Local<Value> Encode(Isolate* isolate,
                    const void* buf,
                    size_t len,
                    enum encoding encoding) {
  return StringBytes::Encode(isolate,
                             static_cast<const char*>(buf),
                             len,
                             encoding);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(Isolate* isolate,
                    Handle<Value> val,
                    enum encoding encoding) {
  HandleScope scope(isolate);

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  return StringBytes::Size(isolate, val, encoding);
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t DecodeWrite(Isolate* isolate,
                    char* buf,
                    size_t buflen,
                    Handle<Value> val,
                    enum encoding encoding) {
  return StringBytes::Write(isolate, buf, buflen, val, encoding, NULL);
}

void AppendExceptionLine(Environment* env,
                         Handle<Value> er,
                         Handle<Message> message) {
  if (message.IsEmpty())
    return;

  HandleScope scope(env->isolate());
  Local<Object> err_obj;
  if (!er.IsEmpty() && er->IsObject()) {
    err_obj = er.As<Object>();

    // Do it only once per message
    if (!err_obj->GetHiddenValue(env->processed_string()).IsEmpty())
      return;
    err_obj->SetHiddenValue(env->processed_string(), True(env->isolate()));
  }

  static char arrow[1024];

  // Print (filename):(line number): (message).
  node::Utf8Value filename(message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber();
  // Print line of source code.
  node::Utf8Value sourceline(message->GetSourceLine());
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

  int start = message->GetStartColumn();
  int end = message->GetEndColumn();

  int off = snprintf(arrow,
                     sizeof(arrow),
                     "%s:%i\n%s\n",
                     filename_string,
                     linenum,
                     sourceline_string);
  assert(off >= 0);

  // Print wavy underline (GetUnderline is deprecated).
  for (int i = 0; i < start; i++) {
    if (sourceline_string[i] == '\0' ||
        static_cast<size_t>(off) >= sizeof(arrow)) {
      break;
    }
    assert(static_cast<size_t>(off) < sizeof(arrow));
    arrow[off++] = (sourceline_string[i] == '\t') ? '\t' : ' ';
  }
  for (int i = start; i < end; i++) {
    if (sourceline_string[i] == '\0' ||
        static_cast<size_t>(off) >= sizeof(arrow)) {
      break;
    }
    assert(static_cast<size_t>(off) < sizeof(arrow));
    arrow[off++] = '^';
  }
  assert(static_cast<size_t>(off - 1) <= sizeof(arrow) - 1);
  arrow[off++] = '\n';
  arrow[off] = '\0';

  Local<String> arrow_str = String::NewFromUtf8(env->isolate(), arrow);
  Local<Value> msg;
  Local<Value> stack;

  // Allocation failed, just print it out
  if (arrow_str.IsEmpty() || err_obj.IsEmpty() || !err_obj->IsNativeError())
    goto print;

  msg = err_obj->Get(env->message_string());
  stack = err_obj->Get(env->stack_string());

  if (msg.IsEmpty() || stack.IsEmpty())
    goto print;

  err_obj->Set(env->message_string(),
               String::Concat(arrow_str, msg->ToString()));
  err_obj->Set(env->stack_string(),
               String::Concat(arrow_str, stack->ToString()));
  return;

 print:
  if (env->printed_error())
    return;
  env->set_printed_error(true);
  uv_tty_reset_mode();
  fprintf(stderr, "\n%s", arrow);
}


static void ReportException(Environment* env,
                            Handle<Value> er,
                            Handle<Message> message) {
  HandleScope scope(env->isolate());

  AppendExceptionLine(env, er, message);

  Local<Value> trace_value;

  if (er->IsUndefined() || er->IsNull())
    trace_value = Undefined(env->isolate());
  else
    trace_value = er->ToObject()->Get(env->stack_string());

  node::Utf8Value trace(trace_value);

  // range errors have a trace member set to undefined
  if (trace.length() > 0 && !trace_value->IsUndefined()) {
    fprintf(stderr, "%s\n", *trace);
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
      node::Utf8Value message(er);
      fprintf(stderr, "%s\n", *message);
    } else {
      node::Utf8Value name_string(name);
      node::Utf8Value message_string(message);
      fprintf(stderr, "%s: %s\n", *name_string, *message_string);
    }
  }

  fflush(stderr);
}


static void ReportException(Environment* env, const TryCatch& try_catch) {
  ReportException(env, try_catch.Exception(), try_catch.Message());
}


// Executes a str within the current v8 context.
static Local<Value> ExecuteString(Environment* env,
                                  Handle<String> source,
                                  Handle<String> filename) {
  EscapableHandleScope scope(env->isolate());
  TryCatch try_catch;

  // try_catch must be nonverbose to disable FatalException() handler,
  // we will handle exceptions ourself.
  try_catch.SetVerbose(false);

  Local<v8::Script> script = v8::Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(env, try_catch);
    exit(3);
  }

  Local<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(env, try_catch);
    exit(4);
  }

  return scope.Escape(result);
}


static void GetActiveRequests(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  Local<Array> ary = Array::New(args.GetIsolate());
  QUEUE* q = NULL;
  int i = 0;

  QUEUE_FOREACH(q, env->req_wrap_queue()) {
    ReqWrap<uv_req_t>* w = ContainerOf(&ReqWrap<uv_req_t>::req_wrap_queue_, q);
    if (w->persistent().IsEmpty())
      continue;
    ary->Set(i++, w->object());
  }

  args.GetReturnValue().Set(ary);
}


// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
void GetActiveHandles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  Local<Array> ary = Array::New(env->isolate());
  QUEUE* q = NULL;
  int i = 0;

  Local<String> owner_sym = env->owner_string();

  QUEUE_FOREACH(q, env->handle_wrap_queue()) {
    HandleWrap* w = ContainerOf(&HandleWrap::handle_wrap_queue_, q);
    if (w->persistent().IsEmpty() || (w->flags_ & HandleWrap::kUnref))
      continue;
    Local<Object> object = w->object();
    Local<Value> owner = object->Get(owner_sym);
    if (owner->IsUndefined())
      owner = object;
    ary->Set(i++, owner);
  }

  args.GetReturnValue().Set(ary);
}


static void Abort(const FunctionCallbackInfo<Value>& args) {
  abort();
}


static void Chdir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() != 1 || !args[0]->IsString()) {
    // FIXME(bnoordhuis) ThrowTypeError?
    return env->ThrowError("Bad argument.");
  }

  node::Utf8Value path(args[0]);
  int err = uv_chdir(*path);
  if (err) {
    return env->ThrowUVException(err, "uv_chdir");
  }
}


static void Cwd(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());
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
      node::Utf8Value str(args[0]);

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
  pp = NULL;

  if (getpwnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != NULL) {
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
  pp = NULL;

  if ((rc = getpwuid_r(uid, &pwd, buf, sizeof(buf), &pp)) == 0 && pp != NULL) {
    return strdup(pp->pw_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return NULL;
}


static gid_t gid_by_name(const char* name) {
  struct group pwd;
  struct group* pp;
  char buf[8192];

  errno = 0;
  pp = NULL;

  if (getgrnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != NULL) {
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
  pp = NULL;

  if ((rc = getgrgid_r(gid, &pwd, buf, sizeof(buf), &pp)) == 0 && pp != NULL) {
    return strdup(pp->gr_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return NULL;
}
#endif


static uid_t uid_by_name(Handle<Value> value) {
  if (value->IsUint32()) {
    return static_cast<uid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(value);
    return uid_by_name(*name);
  }
}


static gid_t gid_by_name(Handle<Value> value) {
  if (value->IsUint32()) {
    return static_cast<gid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(value);
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


static void SetGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("setgid argument must be a number or a string");
  }

  gid_t gid = gid_by_name(args[0]);

  if (gid == gid_not_found) {
    return env->ThrowError("setgid group id does not exist");
  }

  if (setgid(gid)) {
    return env->ThrowErrnoException(errno, "setgid");
  }
}


static void SetUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("setuid argument must be a number or a string");
  }

  uid_t uid = uid_by_name(args[0]);

  if (uid == uid_not_found) {
    return env->ThrowError("setuid user id does not exist");
  }

  if (setuid(uid)) {
    return env->ThrowErrnoException(errno, "setuid");
  }
}


static void GetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int ngroups = getgroups(0, NULL);

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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (!args[0]->IsArray()) {
    return env->ThrowTypeError("argument 1 must be an array");
  }

  Local<Array> groups_list = args[0].As<Array>();
  size_t size = groups_list->Length();
  gid_t* groups = new gid_t[size];

  for (size_t i = 0; i < size; i++) {
    gid_t gid = gid_by_name(groups_list->Get(i));

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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("argument 1 must be a number or a string");
  }

  if (!args[1]->IsUint32() && !args[1]->IsString()) {
    return env->ThrowTypeError("argument 2 must be a number or a string");
  }

  node::Utf8Value arg0(args[0]);
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

  if (user == NULL) {
    return env->ThrowError("initgroups user not found");
  }

  extra_group = gid_by_name(args[1]);

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


void Exit(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());
  exit(args[0]->Int32Value());
}


static void Uptime(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());
  double uptime;

  uv_update_time(env->event_loop());
  uptime = uv_now(env->event_loop()) - prog_start_time;

  args.GetReturnValue().Set(Number::New(env->isolate(), uptime / 1000));
}


void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  size_t rss;
  int err = uv_resident_set_memory(&rss);
  if (err) {
    return env->ThrowUVException(err, "uv_resident_set_memory");
  }

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  env->isolate()->GetHeapStatistics(&v8_heap_stats);

  Local<Integer> heap_total =
      Integer::NewFromUnsigned(env->isolate(), v8_heap_stats.total_heap_size());
  Local<Integer> heap_used =
      Integer::NewFromUnsigned(env->isolate(), v8_heap_stats.used_heap_size());

  Local<Object> info = Object::New(env->isolate());
  info->Set(env->rss_string(), Number::New(env->isolate(), rss));
  info->Set(env->heap_total_string(), heap_total);
  info->Set(env->heap_used_string(), heap_used);

  args.GetReturnValue().Set(info);
}


void Kill(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

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
// so this function instead returns an Array with 2 entries representing seconds
// and nanoseconds, to avoid any integer overflow possibility.
// Pass in an Array from a previous hrtime() call to instead get a time diff.
void Hrtime(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  uint64_t t = uv_hrtime();

  if (args.Length() > 0) {
    // return a time diff tuple
    if (!args[0]->IsArray()) {
      return env->ThrowTypeError(
          "process.hrtime() only accepts an Array tuple.");
    }
    Local<Array> inArray = Local<Array>::Cast(args[0]);
    uint64_t seconds = inArray->Get(0)->Uint32Value();
    uint64_t nanos = inArray->Get(1)->Uint32Value();
    t -= (seconds * NANOS_PER_SEC) + nanos;
  }

  Local<Array> tuple = Array::New(env->isolate(), 2);
  tuple->Set(0, Integer::NewFromUnsigned(env->isolate(), t / NANOS_PER_SEC));
  tuple->Set(1, Integer::NewFromUnsigned(env->isolate(), t % NANOS_PER_SEC));
  args.GetReturnValue().Set(tuple);
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
    // Once node::Init was called we can only register dynamic modules.
    // See DLOpen.
    assert(modpending == NULL);
    modpending = mp;
  }
}

struct node_module* get_builtin_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_builtin; mp != NULL; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  assert(mp == NULL || (mp->nm_flags & NM_F_BUILTIN) != 0);
  return (mp);
}

struct node_module* get_linked_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_linked; mp != NULL; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == NULL || (mp->nm_flags & NM_F_LINKED) != 0);
  return mp;
}

typedef void (UV_DYNAMIC* extInit)(Handle<Object> exports);

// DLOpen is process.dlopen(module, filename).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  struct node_module* mp;
  uv_lib_t lib;

  if (args.Length() < 2) {
    env->ThrowError("process.dlopen takes exactly 2 arguments.");
    return;
  }

  Local<Object> module = args[0]->ToObject();  // Cast
  node::Utf8Value filename(args[1]);  // Cast

  Local<String> exports_string = env->exports_string();
  Local<Object> exports = module->Get(exports_string)->ToObject();

  if (uv_dlopen(*filename, &lib)) {
    Local<String> errmsg = OneByteString(env->isolate(), uv_dlerror(&lib));
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, args[1]->ToString());
#endif  // _WIN32
    env->isolate()->ThrowException(Exception::Error(errmsg));
    return;
  }

  /*
   * Objects containing v14 or later modules will have registered themselves
   * on the pending list.  Activate all of them now.  At present, only one
   * module per object is supported.
   */
  mp = modpending;
  modpending = NULL;

  if (mp == NULL) {
    env->ThrowError("Module did not self-register.");
    return;
  }
  if (mp->nm_version != NODE_MODULE_VERSION) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "Module version mismatch. Expected %d, got %d.",
             NODE_MODULE_VERSION, mp->nm_version);
    env->ThrowError(errmsg);
    return;
  }
  if (mp->nm_flags & NM_F_BUILTIN) {
    env->ThrowError("Built-in module self-registered.");
    return;
  }

  mp->nm_dso_handle = lib.handle;
  mp->nm_link = modlist_addon;
  modlist_addon = mp;

  if (mp->nm_context_register_func != NULL) {
    mp->nm_context_register_func(exports, module, env->context(), mp->nm_priv);
  } else if (mp->nm_register_func != NULL) {
    mp->nm_register_func(exports, module, mp->nm_priv);
  } else {
    env->ThrowError("Module has no declared entry point.");
    return;
  }

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
}


static void OnFatalError(const char* location, const char* message) {
  if (location) {
    fprintf(stderr, "FATAL ERROR: %s %s\n", location, message);
  } else {
    fprintf(stderr, "FATAL ERROR: %s\n", message);
  }
  fflush(stderr);
  abort();
}


NO_RETURN void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to supress compiler warning
  abort();
}


void FatalException(Isolate* isolate,
                    Handle<Value> error,
                    Handle<Message> message) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Function> fatal_exception_function =
      process_object->Get(fatal_exception_string).As<Function>();

  if (!fatal_exception_function->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(env, error, message);
    exit(6);
  }

  TryCatch fatal_try_catch;

  // Do not call FatalException when _fatalException handler throws
  fatal_try_catch.SetVerbose(false);

  // this will return true if the JS layer handled it, false otherwise
  Local<Value> caught =
      fatal_exception_function->Call(process_object, 1, &error);

  if (fatal_try_catch.HasCaught()) {
    // the fatal exception function threw, so we must exit
    ReportException(env, fatal_try_catch);
    exit(7);
  }

  if (false == caught->BooleanValue()) {
    ReportException(env, error, message);
    exit(1);
  }
}


void FatalException(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope scope(isolate);
  // TODO(bajtos) do not call FatalException if try_catch is verbose
  // (requires V8 API to expose getter for try_catch.is_verbose_)
  FatalException(isolate, try_catch.Exception(), try_catch.Message());
}


void OnMessage(Handle<Message> message, Handle<Value> error) {
  // The current version of V8 sends messages for errors only
  // (thus `error` is always set).
  FatalException(Isolate::GetCurrent(), error, message);
}


static void Binding(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  Local<String> module = args[0]->ToString();
  node::Utf8Value module_v(module);

  Local<Object> cache = env->binding_cache_object();
  Local<Object> exports;

  if (cache->Has(module)) {
    exports = cache->Get(module)->ToObject();
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
  if (mod != NULL) {
    exports = Object::New(env->isolate());
    // Internal bindings don't have a "module" object, only exports.
    assert(mod->nm_register_func == NULL);
    assert(mod->nm_context_register_func != NULL);
    Local<Value> unused = Undefined(env->isolate());
    mod->nm_context_register_func(exports, unused,
      env->context(), mod->nm_priv);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New(env->isolate());
    DefineConstants(exports);
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

  Local<String> module = args[0]->ToString();

  Local<Object> cache = env->binding_cache_object();
  Local<Value> exports_v = cache->Get(module);

  if (exports_v->IsObject())
    return args.GetReturnValue().Set(exports_v.As<Object>());

  node::Utf8Value module_v(module);
  node_module* mod = get_linked_module(*module_v);

  if (mod == NULL) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "No such module was linked: %s",
             *module_v);
    return env->ThrowError(errmsg);
  }

  Local<Object> exports = Object::New(env->isolate());

  if (mod->nm_context_register_func != NULL) {
    mod->nm_context_register_func(exports,
                                  module,
                                  env->context(),
                                  mod->nm_priv);
  } else if (mod->nm_register_func != NULL) {
    mod->nm_register_func(exports, module, mod->nm_priv);
  } else {
    return env->ThrowError("Linked module has no declared entry point.");
  }

  cache->Set(module, exports);

  args.GetReturnValue().Set(exports);
}

static void ProcessTitleGetter(Local<String> property,
                               const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  char buffer[512];
  uv_get_process_title(buffer, sizeof(buffer));
  info.GetReturnValue().Set(String::NewFromUtf8(env->isolate(), buffer));
}


static void ProcessTitleSetter(Local<String> property,
                               Local<Value> value,
                               const PropertyCallbackInfo<void>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  node::Utf8Value title(value);
  // TODO(piscisaureus): protect with a lock
  uv_set_process_title(*title);
}


static void EnvGetter(Local<String> property,
                      const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
#ifdef __POSIX__
  node::Utf8Value key(property);
  const char* val = getenv(*key);
  if (val) {
    return info.GetReturnValue().Set(String::NewFromUtf8(env->isolate(), val));
  }
#else  // _WIN32
  String::Value key(property);
  WCHAR buffer[32767];  // The maximum size allowed for environment variables.
  DWORD result = GetEnvironmentVariableW(reinterpret_cast<WCHAR*>(*key),
                                         buffer,
                                         ARRAY_SIZE(buffer));
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // not found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) &&
      result < ARRAY_SIZE(buffer)) {
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(buffer);
    Local<String> rc = String::NewFromTwoByte(env->isolate(), two_byte_buffer);
    return info.GetReturnValue().Set(rc);
  }
#endif
  // Not found.  Fetch from prototype.
  info.GetReturnValue().Set(
      info.Data().As<Object>()->Get(property));
}


static void EnvSetter(Local<String> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
#ifdef __POSIX__
  node::Utf8Value key(property);
  node::Utf8Value val(value);
  setenv(*key, *val, 1);
#else  // _WIN32
  String::Value key(property);
  String::Value val(value);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  // Environment variables that start with '=' are read-only.
  if (key_ptr[0] != L'=') {
    SetEnvironmentVariableW(key_ptr, reinterpret_cast<WCHAR*>(*val));
  }
#endif
  // Whether it worked or not, always return rval.
  info.GetReturnValue().Set(value);
}


static void EnvQuery(Local<String> property,
                     const PropertyCallbackInfo<Integer>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  int32_t rc = -1;  // Not found unless proven otherwise.
#ifdef __POSIX__
  node::Utf8Value key(property);
  if (getenv(*key))
    rc = 0;
#else  // _WIN32
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (GetEnvironmentVariableW(key_ptr, NULL, 0) > 0 ||
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


static void EnvDeleter(Local<String> property,
                       const PropertyCallbackInfo<Boolean>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  bool rc = true;
#ifdef __POSIX__
  node::Utf8Value key(property);
  rc = getenv(*key) != NULL;
  if (rc)
    unsetenv(*key);
#else
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (key_ptr[0] == L'=' || !SetEnvironmentVariableW(key_ptr, NULL)) {
    // Deletion failed. Return true if the key wasn't there in the first place,
    // false if it is still there.
    rc = GetEnvironmentVariableW(key_ptr, NULL, NULL) == 0 &&
         GetLastError() != ERROR_SUCCESS;
  }
#endif
  info.GetReturnValue().Set(rc);
}


static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
#ifdef __POSIX__
  int size = 0;
  while (environ[size])
    size++;

  Local<Array> envarr = Array::New(env->isolate(), size);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    Local<String> name = String::NewFromUtf8(env->isolate(),
                                             var,
                                             String::kNormalString,
                                             length);
    envarr->Set(i, name);
  }
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == NULL)
    return;  // This should not happen.
  Local<Array> envarr = Array::New(env->isolate());
  WCHAR* p = environment;
  int i = 0;
  while (*p != NULL) {
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
    Local<String> value = String::NewFromTwoByte(env->isolate(),
                                                 two_byte_buffer,
                                                 String::kNormalString,
                                                 two_byte_buffer_len);
    envarr->Set(i++, value);
    p = s + wcslen(s) + 1;
  }
  FreeEnvironmentStringsW(environment);
#endif

  info.GetReturnValue().Set(envarr);
}


static Handle<Object> GetFeatures(Environment* env) {
  EscapableHandleScope scope(env->isolate());

  Local<Object> obj = Object::New(env->isolate());
#if defined(DEBUG) && DEBUG
  Local<Value> debug = True(env->isolate());
#else
  Local<Value> debug = False(env->isolate());
#endif  // defined(DEBUG) && DEBUG

  obj->Set(env->debug_string(), debug);

  obj->Set(env->uv_string(), True(env->isolate()));
  // TODO(bnoordhuis) ping libuv
  obj->Set(env->ipv6_lc_string(), True(env->isolate()));

#ifdef OPENSSL_NPN_NEGOTIATED
  Local<Boolean> tls_npn = True(env->isolate());
#else
  Local<Boolean> tls_npn = False(env->isolate());
#endif
  obj->Set(env->tls_npn_string(), tls_npn);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  Local<Boolean> tls_sni = True(env->isolate());
#else
  Local<Boolean> tls_sni = False(env->isolate());
#endif
  obj->Set(env->tls_sni_string(), tls_sni);

#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  Local<Boolean> tls_ocsp = True(env->isolate());
#else
  Local<Boolean> tls_ocsp = False(env->isolate());
#endif  // !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  obj->Set(env->tls_ocsp_string(), tls_ocsp);

  obj->Set(env->tls_string(),
           Boolean::New(env->isolate(), get_builtin_module("crypto") != NULL));

  return scope.Escape(obj);
}


static void DebugPortGetter(Local<String> property,
                            const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  info.GetReturnValue().Set(debug_port);
}


static void DebugPortSetter(Local<String> property,
                            Local<Value> value,
                            const PropertyCallbackInfo<void>& info) {
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope scope(env->isolate());
  debug_port = value->Int32Value();
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args);
static void DebugPause(const FunctionCallbackInfo<Value>& args);
static void DebugEnd(const FunctionCallbackInfo<Value>& args);


void NeedImmediateCallbackGetter(Local<String> property,
                                 const PropertyCallbackInfo<Value>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  const uv_check_t* immediate_check_handle = env->immediate_check_handle();
  bool active = uv_is_active(
      reinterpret_cast<const uv_handle_t*>(immediate_check_handle));
  info.GetReturnValue().Set(active);
}


static void NeedImmediateCallbackSetter(
    Local<String> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info) {
  HandleScope handle_scope(info.GetIsolate());
  Environment* env = Environment::GetCurrent(info.GetIsolate());

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


void SetIdle(uv_prepare_t* handle) {
  Environment* env = Environment::from_idle_prepare_handle(handle);
  env->isolate()->GetCpuProfiler()->SetIdle(true);
}


void ClearIdle(uv_check_t* handle) {
  Environment* env = Environment::from_idle_check_handle(handle);
  env->isolate()->GetCpuProfiler()->SetIdle(false);
}


void StartProfilerIdleNotifier(Environment* env) {
  uv_prepare_start(env->idle_prepare_handle(), SetIdle);
  uv_check_start(env->idle_check_handle(), ClearIdle);
}


void StopProfilerIdleNotifier(Environment* env) {
  uv_prepare_stop(env->idle_prepare_handle());
  uv_check_stop(env->idle_check_handle());
}


void StartProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  StartProfilerIdleNotifier(env);
}


void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  StopProfilerIdleNotifier(env);
}


#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->ForceSet(OneByteString(env->isolate(), str), var, v8::ReadOnly);     \
  } while (0)


void SetupProcessObject(Environment* env,
                        int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv) {
  HandleScope scope(env->isolate());

  Local<Object> process = env->process_object();

  process->SetAccessor(env->title_string(),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

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
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR);
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

  const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);
  READONLY_PROPERTY(
      versions,
      "modules",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_modules_version));

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
  READONLY_PROPERTY(process, "arch", OneByteString(env->isolate(), ARCH));

  // process.platform
  READONLY_PROPERTY(process,
                    "platform",
                    OneByteString(env->isolate(), PLATFORM));

  // process.argv
  Local<Array> arguments = Array::New(env->isolate(), argc);
  for (int i = 0; i < argc; ++i) {
    arguments->Set(i, String::NewFromUtf8(env->isolate(), argv[i]));
  }
  process->Set(env->argv_string(), arguments);

  // process.execArgv
  Local<Array> exec_arguments = Array::New(env->isolate(), exec_argc);
  for (int i = 0; i < exec_argc; ++i) {
    exec_arguments->Set(i, String::NewFromUtf8(env->isolate(), exec_argv[i]));
  }
  process->Set(env->exec_argv_string(), exec_arguments);

  // create process.env
  Local<ObjectTemplate> process_env_template =
      ObjectTemplate::New(env->isolate());
  process_env_template->SetNamedPropertyHandler(EnvGetter,
                                                EnvSetter,
                                                EnvQuery,
                                                EnvDeleter,
                                                EnvEnumerator,
                                                Object::New(env->isolate()));
  Local<Object> process_env = process_env_template->NewInstance();
  process->Set(env->env_string(), process_env);

  READONLY_PROPERTY(process, "pid", Integer::New(env->isolate(), getpid()));
  READONLY_PROPERTY(process, "features", GetFeatures(env));
  process->SetAccessor(env->need_imm_cb_string(),
      NeedImmediateCallbackGetter,
      NeedImmediateCallbackSetter);

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

  // -i, --interactive
  if (force_repl) {
    READONLY_PROPERTY(process, "_forceRepl", True(env->isolate()));
  }

  // --no-deprecation
  if (no_deprecation) {
    READONLY_PROPERTY(process, "noDeprecation", True(env->isolate()));
  }

  // --throw-deprecation
  if (throw_deprecation) {
    READONLY_PROPERTY(process, "throwDeprecation", True(env->isolate()));
  }

  // --trace-deprecation
  if (trace_deprecation) {
    READONLY_PROPERTY(process, "traceDeprecation", True(env->isolate()));
  }

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
  process->Set(env->exec_path_string(), exec_path_value);
  delete[] exec_path;

  process->SetAccessor(env->debug_port_string(),
                       DebugPortGetter,
                       DebugPortSetter);

  // define various internal methods
  NODE_SET_METHOD(process,
                  "_startProfilerIdleNotifier",
                  StartProfilerIdleNotifier);
  NODE_SET_METHOD(process,
                  "_stopProfilerIdleNotifier",
                  StopProfilerIdleNotifier);
  NODE_SET_METHOD(process, "_getActiveRequests", GetActiveRequests);
  NODE_SET_METHOD(process, "_getActiveHandles", GetActiveHandles);
  NODE_SET_METHOD(process, "reallyExit", Exit);
  NODE_SET_METHOD(process, "abort", Abort);
  NODE_SET_METHOD(process, "chdir", Chdir);
  NODE_SET_METHOD(process, "cwd", Cwd);

  NODE_SET_METHOD(process, "umask", Umask);

#if defined(__POSIX__) && !defined(__ANDROID__)
  NODE_SET_METHOD(process, "getuid", GetUid);
  NODE_SET_METHOD(process, "setuid", SetUid);

  NODE_SET_METHOD(process, "setgid", SetGid);
  NODE_SET_METHOD(process, "getgid", GetGid);

  NODE_SET_METHOD(process, "getgroups", GetGroups);
  NODE_SET_METHOD(process, "setgroups", SetGroups);
  NODE_SET_METHOD(process, "initgroups", InitGroups);
#endif  // __POSIX__ && !defined(__ANDROID__)

  NODE_SET_METHOD(process, "_kill", Kill);

  NODE_SET_METHOD(process, "_debugProcess", DebugProcess);
  NODE_SET_METHOD(process, "_debugPause", DebugPause);
  NODE_SET_METHOD(process, "_debugEnd", DebugEnd);

  NODE_SET_METHOD(process, "hrtime", Hrtime);

  NODE_SET_METHOD(process, "dlopen", DLOpen);

  NODE_SET_METHOD(process, "uptime", Uptime);
  NODE_SET_METHOD(process, "memoryUsage", MemoryUsage);

  NODE_SET_METHOD(process, "binding", Binding);
  NODE_SET_METHOD(process, "_linkedBinding", LinkedBinding);

  NODE_SET_METHOD(process, "_setupNextTick", SetupNextTick);
  NODE_SET_METHOD(process, "_setupDomainUse", SetupDomainUse);

  // pre-set _events object for faster emit checks
  process->Set(env->events_string(), Object::New(env->isolate()));
}


#undef READONLY_PROPERTY


static void AtExit() {
  uv_tty_reset_mode();
}


static void SignalExit(int signo) {
  uv_tty_reset_mode();
  raise(signo);
}


// Most of the time, it's best to use `console.error` to write
// to the process.stderr stream.  However, in some cases, such as
// when debugging the stream.Writable class or the process.nextTick
// function, it is useful to bypass JavaScript entirely.
static void RawDebug(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  assert(args.Length() == 1 && args[0]->IsString() &&
         "must be called with a single string");

  node::Utf8Value message(args[0]);
  fprintf(stderr, "%s\n", *message);
  fflush(stderr);
}


void LoadEnvironment(Environment* env) {
  HandleScope handle_scope(env->isolate());

  V8::SetFatalErrorHandler(node::OnFatalError);
  V8::AddMessageListener(OnMessage);

  // Compile, execute the src/node.js file. (Which was included as static C
  // string in node_natives.h. 'natve_node' is the string containing that
  // source code.)

  // The node.js file returns a function 'f'
  atexit(AtExit);

  TryCatch try_catch;

  // Disable verbose mode to stop FatalException() handler from trying
  // to handle the exception. Errors this early in the start-up phase
  // are not safe to ignore.
  try_catch.SetVerbose(false);

  Local<String> script_name = FIXED_ONE_BYTE_STRING(env->isolate(), "node.js");
  Local<Value> f_value = ExecuteString(env, MainSource(env), script_name);
  if (try_catch.HasCaught())  {
    ReportException(env, try_catch);
    exit(10);
  }
  assert(f_value->IsFunction());
  Local<Function> f = Local<Function>::Cast(f_value);

  // Now we call 'f' with the 'process' variable that we've built up with
  // all our bindings. Inside node.js we'll take care of assigning things to
  // their places.

  // We start the process this way in order to be more modular. Developers
  // who do not like how 'src/node.js' setups the module system but do like
  // Node's I/O bindings may want to replace 'f' with their own function.

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
  // to attach the debugger fast enought to break on exception
  // thrown during process startup.
  try_catch.SetVerbose(true);

  NODE_SET_METHOD(env->process_object(), "_rawDebug", RawDebug);

  Local<Value> arg = env->process_object();
  f->Call(global, 1, &arg);
}

static void PrintHelp();

static bool ParseDebugOpt(const char* arg) {
  const char* port = NULL;

  if (!strcmp(arg, "--debug")) {
    use_debug_agent = true;
  } else if (!strncmp(arg, "--debug=", sizeof("--debug=") - 1)) {
    use_debug_agent = true;
    port = arg + sizeof("--debug=") - 1;
  } else if (!strcmp(arg, "--debug-brk")) {
    use_debug_agent = true;
    debug_wait_connect = true;
  } else if (!strncmp(arg, "--debug-brk=", sizeof("--debug-brk=") - 1)) {
    use_debug_agent = true;
    debug_wait_connect = true;
    port = arg + sizeof("--debug-brk=") - 1;
  } else if (!strncmp(arg, "--debug-port=", sizeof("--debug-port=") - 1)) {
    port = arg + sizeof("--debug-port=") - 1;
  } else {
    return false;
  }

  if (port != NULL) {
    debug_port = atoi(port);
    if (debug_port < 1024 || debug_port > 65535) {
      fprintf(stderr, "Debug port must be in range 1024 to 65535.\n");
      PrintHelp();
      exit(12);
    }
  }

  return true;
}

static void PrintHelp() {
  printf("Usage: node [options] [ -e script | script.js ] [arguments] \n"
         "       node debug script.js [arguments] \n"
         "\n"
         "Options:\n"
         "  -v, --version        print node's version\n"
         "  -e, --eval script    evaluate script\n"
         "  -p, --print          evaluate script and print result\n"
         "  -i, --interactive    always enter the REPL even if stdin\n"
         "                       does not appear to be a terminal\n"
         "  --no-deprecation     silence deprecation warnings\n"
         "  --throw-deprecation  throw an exception anytime a deprecated "
         "function is used\n"
         "  --trace-deprecation  show stack traces on deprecations\n"
         "  --v8-options         print v8 command line options\n"
         "  --max-stack-size=val set max v8 stack size (bytes)\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         "  --icu-data-dir=dir   set ICU data load path to dir\n"
         "                         (overrides NODE_ICU_DATA)\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                       Note: linked-in ICU data is\n"
         "                       present.\n"
#endif
#endif
         "  --enable-ssl2        enable ssl2\n"
         "  --enable-ssl3        enable ssl3\n"
         "\n"
         "Environment variables:\n"
#ifdef _WIN32
         "NODE_PATH              ';'-separated list of directories\n"
#else
         "NODE_PATH              ':'-separated list of directories\n"
#endif
         "                       prefixed to the module search path.\n"
         "NODE_MODULE_CONTEXTS   Set to 1 to load modules in their own\n"
         "                       global contexts.\n"
         "NODE_DISABLE_COLORS    Set to 1 to disable colors in the REPL\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         "NODE_ICU_DATA          Data path for ICU (Intl object) data\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                       (will extend linked-in data)\n"
#endif
#endif
         "\n"
         "Documentation can be found at http://nodejs.org/\n");
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

  for (unsigned int i = 0; i < nargs; ++i) {
    new_exec_argv[i] = NULL;
    new_v8_argv[i] = NULL;
    new_argv[i] = NULL;
  }

  // exec_argv starts with the first option, the other two start with argv[0].
  unsigned int new_exec_argc = 0;
  unsigned int new_v8_argc = 1;
  unsigned int new_argc = 1;
  new_v8_argv[0] = argv[0];
  new_argv[0] = argv[0];

  unsigned int index = 1;
  while (index < nargs && argv[index][0] == '-') {
    const char* const arg = argv[index];
    unsigned int args_consumed = 1;

    if (ParseDebugOpt(arg)) {
      // Done, consumed by ParseDebugOpt().
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--enable-ssl2") == 0) {
      SSL2_ENABLE = true;
    } else if (strcmp(arg, "--enable-ssl3") == 0) {
      SSL3_ENABLE = true;
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0 ||
               strcmp(arg, "-e") == 0 ||
               strcmp(arg, "--print") == 0 ||
               strcmp(arg, "-pe") == 0 ||
               strcmp(arg, "-p") == 0) {
      bool is_eval = strchr(arg, 'e') != NULL;
      bool is_print = strchr(arg, 'p') != NULL;
      print_eval = print_eval || is_print;
      // --eval, -e and -pe always require an argument.
      if (is_eval == true) {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (eval_string == NULL) {
          fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
          exit(9);
        }
      } else if ((index + 1 < nargs) &&
                 argv[index + 1] != NULL &&
                 argv[index + 1][0] != '-') {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (strncmp(eval_string, "\\-", 2) == 0) {
          // Starts with "\\-": escaped expression, drop the backslash.
          eval_string += 1;
        }
      }
    } else if (strcmp(arg, "--interactive") == 0 || strcmp(arg, "-i") == 0) {
      force_repl = true;
    } else if (strcmp(arg, "--no-deprecation") == 0) {
      no_deprecation = true;
    } else if (strcmp(arg, "--trace-deprecation") == 0) {
      trace_deprecation = true;
    } else if (strcmp(arg, "--throw-deprecation") == 0) {
      throw_deprecation = true;
    } else if (strcmp(arg, "--v8-options") == 0) {
      new_v8_argv[new_v8_argc] = "--help";
      new_v8_argc += 1;
#if defined(NODE_HAVE_I18N_SUPPORT)
    } else if (strncmp(arg, "--icu-data-dir=", 15) == 0) {
      icu_data_dir = arg + 15;
#endif
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
}


// Called from V8 Debug Agent TCP thread.
static void DispatchMessagesDebugAgentCallback(Environment* env) {
  // TODO(indutny): move async handle to environment
  uv_async_send(&dispatch_debug_messages_async);
}


static void StartDebug(Environment* env, bool wait) {
  CHECK(!debugger_running);

  env->debugger_agent()->set_dispatch_handler(
        DispatchMessagesDebugAgentCallback);
  debugger_running = env->debugger_agent()->Start(debug_port, wait);
  if (debugger_running == false) {
    fprintf(stderr, "Starting debugger on port %d failed\n", debug_port);
    fflush(stderr);
    return;
  }
}


// Called from the main thread.
static void EnableDebug(Environment* env) {
  CHECK(debugger_running);

  // Send message to enable debug in workers
  HandleScope handle_scope(env->isolate());

  Local<Object> message = Object::New(env->isolate());
  message->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "cmd"),
               FIXED_ONE_BYTE_STRING(env->isolate(), "NODE_DEBUG_ENABLED"));
  Local<Value> argv[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "internalMessage"),
    message
  };
  MakeCallback(env, env->process_object(), "emit", ARRAY_SIZE(argv), argv);

  // Enabled debugger, possibly making it wait on a semaphore
  env->debugger_agent()->Enable();
}


// Called from the main thread.
static void DispatchDebugMessagesAsyncCallback(uv_async_t* handle) {
  if (debugger_running == false) {
    fprintf(stderr, "Starting debugger agent.\n");

    Environment* env = Environment::GetCurrent(node_isolate);
    Context::Scope context_scope(env->context());

    StartDebug(env, false);
    EnableDebug(env);
  }
  Isolate::Scope isolate_scope(node_isolate);
  v8::Debug::ProcessDebugMessages();
}


#ifdef __POSIX__
static volatile sig_atomic_t caught_early_debug_signal;


static void EarlyDebugSignalHandler(int signo) {
  caught_early_debug_signal = 1;
}


static void InstallEarlyDebugSignalHandler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = EarlyDebugSignalHandler;
  sigaction(SIGUSR1, &sa, NULL);
}


static void EnableDebugSignalHandler(int signo) {
  // Call only async signal-safe functions here!
  v8::Debug::DebugBreak(*static_cast<Isolate* volatile*>(&node_isolate));
  uv_async_send(&dispatch_debug_messages_async);
}


static void RegisterSignalHandler(int signal,
                                  void (*handler)(int signal),
                                  bool reset_handler = false) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sa.sa_flags = reset_handler ? SA_RESETHAND : 0;
  sigfillset(&sa.sa_mask);
  CHECK_EQ(sigaction(signal, &sa, NULL), 0);
}


void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

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


static int RegisterDebugSignalHandler() {
  // FIXME(bnoordhuis) Should be per-isolate or per-context, not global.
  RegisterSignalHandler(SIGUSR1, EnableDebugSignalHandler);
  // If we caught a SIGUSR1 during the bootstrap process, re-raise it
  // now that the debugger infrastructure is in place.
  if (caught_early_debug_signal)
    raise(SIGUSR1);
  return 0;
}
#endif  // __POSIX__


#ifdef _WIN32
DWORD WINAPI EnableDebugThreadProc(void* arg) {
  v8::Debug::DebugBreak(*static_cast<Isolate* volatile*>(&node_isolate));
  uv_async_send(&dispatch_debug_messages_async);
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
                                       ARRAY_SIZE(mapping_name)) < 0) {
    return -1;
  }

  mapping_handle = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                      NULL,
                                      PAGE_READWRITE,
                                      0,
                                      sizeof *handler,
                                      mapping_name);
  if (mapping_handle == NULL) {
    return -1;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping_handle,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    sizeof *handler));
  if (handler == NULL) {
    CloseHandle(mapping_handle);
    return -1;
  }

  *handler = EnableDebugThreadProc;

  UnmapViewOfFile(static_cast<void*>(handler));

  return 0;
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  HandleScope scope(isolate);
  DWORD pid;
  HANDLE process = NULL;
  HANDLE thread = NULL;
  HANDLE mapping = NULL;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = NULL;

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
  if (process == NULL) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "OpenProcess"));
    goto out;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       ARRAY_SIZE(mapping_name)) < 0) {
    env->ThrowErrnoException(errno, "sprintf");
    goto out;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == NULL) {
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
  if (handler == NULL || *handler == NULL) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "MapViewOfFile"));
    goto out;
  }

  thread = CreateRemoteThread(process,
                              NULL,
                              0,
                              *handler,
                              NULL,
                              0,
                              NULL);
  if (thread == NULL) {
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
  if (process != NULL)
    CloseHandle(process);
  if (thread != NULL)
    CloseHandle(thread);
  if (handler != NULL)
    UnmapViewOfFile(handler);
  if (mapping != NULL)
    CloseHandle(mapping);
}
#endif  // _WIN32


static void DebugPause(const FunctionCallbackInfo<Value>& args) {
  v8::Debug::DebugBreak(args.GetIsolate());
}


static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
  if (debugger_running) {
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    env->debugger_agent()->Stop();
    debugger_running = false;
  }
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
  // FIXME(bnoordhuis) Should be per-isolate or per-context, not global.
  uv_async_init(uv_default_loop(),
                &dispatch_debug_messages_async,
                DispatchDebugMessagesAsyncCallback);
  uv_unref(reinterpret_cast<uv_handle_t*>(&dispatch_debug_messages_async));

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

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

#if defined(NODE_HAVE_I18N_SUPPORT)
  if (icu_data_dir == NULL) {
    // if the parameter isn't given, use the env variable.
    icu_data_dir = getenv("NODE_ICU_DATA");
  }
  // Initialize ICU.
  // If icu_data_dir is NULL here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(icu_data_dir)) {
    FatalError(NULL, "Could not initialize ICU "
                     "(check NODE_ICU_DATA or --icu-data-dir parameters)");
  }
#endif
  // The const_cast doesn't violate conceptual const-ness.  V8 doesn't modify
  // the argv array or the elements it points to.
  V8::SetFlagsFromCommandLine(&v8_argc, const_cast<char**>(v8_argv), true);

  // Anything that's still in v8_argv is not a V8 or a node option.
  for (int i = 1; i < v8_argc; i++) {
    fprintf(stderr, "%s: bad option: %s\n", argv[0], v8_argv[i]);
  }
  delete[] v8_argv;
  v8_argv = NULL;

  if (v8_argc > 1) {
    exit(9);
  }

  if (debug_wait_connect) {
    const char expose_debug_as[] = "--expose_debug_as=v8debug";
    V8::SetFlagsFromString(expose_debug_as, sizeof(expose_debug_as) - 1);
  }

  V8::SetArrayBufferAllocator(&ArrayBufferAllocator::the_singleton);

  // Fetch a reference to the main isolate, so we have a reference to it
  // even when we need it to access it from another (debugger) thread.
  node_isolate = Isolate::New();
  Isolate::Scope isolate_scope(node_isolate);

#ifdef __POSIX__
  // Raise the open file descriptor limit.
  {  // NOLINT (whitespace/braces)
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
  }
  // Ignore SIGPIPE
  RegisterSignalHandler(SIGPIPE, SIG_IGN);
  RegisterSignalHandler(SIGINT, SignalExit, true);
  RegisterSignalHandler(SIGTERM, SignalExit, true);
#endif  // __POSIX__

  if (!use_debug_agent) {
    RegisterDebugSignalHandler();
  }
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
  at_exit_functions_ = NULL;

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
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Object> process_object = env->process_object();
  Local<String> exit_code = FIXED_ONE_BYTE_STRING(env->isolate(), "exitCode");
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "beforeExit"),
    process_object->Get(exit_code)->ToInteger()
  };
  MakeCallback(env, process_object, "emit", ARRAY_SIZE(args), args);
}


int EmitExit(Environment* env) {
  // process.emit('exit')
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  process_object->Set(env->exiting_string(), True(env->isolate()));

  Handle<String> exitCode = env->exit_code_string();
  int code = process_object->Get(exitCode)->Int32Value();

  Local<Value> args[] = {
    env->exit_string(),
    Integer::New(env->isolate(), code)
  };

  MakeCallback(env, process_object, "emit", ARRAY_SIZE(args), args);

  // Reload exit code, it may be changed by `emit('exit')`
  return process_object->Get(exitCode)->Int32Value();
}


// Just a convenience method
Environment* CreateEnvironment(Isolate* isolate,
                               Handle<Context> context,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  Environment* env;
  Context::Scope context_scope(context);

  env = CreateEnvironment(isolate,
                          uv_default_loop(),
                          context,
                          argc,
                          argv,
                          exec_argc,
                          exec_argv);

  LoadEnvironment(env);

  return env;
}


static void HandleCloseCb(uv_handle_t* handle) {
  Environment* env = reinterpret_cast<Environment*>(handle->data);
  env->FinishHandleCleanup(handle);
}


static void HandleCleanup(Environment* env,
                          uv_handle_t* handle,
                          void* arg) {
  handle->data = env;
  uv_close(handle, HandleCloseCb);
}


Environment* CreateEnvironment(Isolate* isolate,
                               uv_loop_t* loop,
                               Handle<Context> context,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  HandleScope handle_scope(isolate);

  Context::Scope context_scope(context);
  Environment* env = Environment::New(context, loop);

  isolate->SetAutorunMicrotasks(false);

  uv_check_init(env->event_loop(), env->immediate_check_handle());
  uv_unref(
      reinterpret_cast<uv_handle_t*>(env->immediate_check_handle()));

  uv_idle_init(env->event_loop(), env->immediate_idle_handle());

  // Inform V8's CPU profiler when we're idle.  The profiler is sampling-based
  // but not all samples are created equal; mark the wall clock time spent in
  // epoll_wait() and friends so profiling tools can filter it out.  The samples
  // still end up in v8.log but with state=IDLE rather than state=EXTERNAL.
  // TODO(bnoordhuis) Depends on a libuv implementation detail that we should
  // probably fortify in the API contract, namely that the last started prepare
  // or check watcher runs first.  It's not 100% foolproof; if an add-on starts
  // a prepare or check watcher after us, any samples attributed to its callback
  // will be recorded with state=IDLE.
  uv_prepare_init(env->event_loop(), env->idle_prepare_handle());
  uv_check_init(env->event_loop(), env->idle_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(env->idle_prepare_handle()));
  uv_unref(reinterpret_cast<uv_handle_t*>(env->idle_check_handle()));

  // Register handle cleanups
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->immediate_check_handle()),
      HandleCleanup,
      NULL);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->immediate_idle_handle()),
      HandleCleanup,
      NULL);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->idle_prepare_handle()),
      HandleCleanup,
      NULL);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->idle_check_handle()),
      HandleCleanup,
      NULL);

  if (v8_is_profiling) {
    StartProfilerIdleNotifier(env);
  }

  Local<FunctionTemplate> process_template = FunctionTemplate::New(isolate);
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "process"));

  Local<Object> process_object = process_template->GetFunction()->NewInstance();
  env->set_process_object(process_object);

  SetupProcessObject(env, argc, argv, exec_argc, exec_argv);

  return env;
}


int Start(int argc, char** argv) {
  const char* replaceInvalid = getenv("NODE_INVALID_UTF8");

  if (replaceInvalid == NULL)
    WRITE_UTF8_FLAGS |= String::REPLACE_INVALID_UTF8;

#if !defined(_WIN32)
  // Try hard not to lose SIGUSR1 signals during the bootstrap process.
  InstallEarlyDebugSignalHandler();
#endif

  assert(argc > 0);

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  // This needs to run *before* V8::Initialize().  The const_cast is not
  // optional, in case you're wondering.
  int exec_argc;
  const char** exec_argv;
  Init(&argc, const_cast<const char**>(argv), &exec_argc, &exec_argv);

#if HAVE_OPENSSL
  // V8 on Windows doesn't have a good source of entropy. Seed it from
  // OpenSSL's pool.
  V8::SetEntropySource(crypto::EntropySource);
#endif

  int code;
  V8::Initialize();
  node_is_initialized = true;
  {
    Locker locker(node_isolate);
    Isolate::Scope isolate_scope(node_isolate);
    HandleScope handle_scope(node_isolate);
    Local<Context> context = Context::New(node_isolate);
    Environment* env = CreateEnvironment(
        node_isolate,
        uv_default_loop(),
        context,
        argc,
        argv,
        exec_argc,
        exec_argv);
    Context::Scope context_scope(context);

    // Start debug agent when argv has --debug
    if (use_debug_agent)
      StartDebug(env, debug_wait_connect);

    LoadEnvironment(env);

    // Enable debugger
    if (use_debug_agent)
      EnableDebug(env);

    bool more;
    do {
      more = uv_run(env->event_loop(), UV_RUN_ONCE);
      if (more == false) {
        EmitBeforeExit(env);

        // Emit `beforeExit` if the loop became alive either after emitting
        // event, or after running some callbacks.
        more = uv_loop_alive(env->event_loop());
        if (uv_run(env->event_loop(), UV_RUN_NOWAIT) != 0)
          more = true;
      }
    } while (more == true);
    code = EmitExit(env);
    RunAtExit(env);

    env->Dispose();
    env = NULL;
  }

  CHECK_NE(node_isolate, NULL);
  node_isolate->Dispose();
  node_isolate = NULL;
  V8::Dispose();

  delete[] exec_argv;
  exec_argv = NULL;

  return code;
}


}  // namespace node

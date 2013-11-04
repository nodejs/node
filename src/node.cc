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
using v8::ThrowException;
using v8::TryCatch;
using v8::Uint32;
using v8::V8;
using v8::Value;
using v8::kExternalUnsignedIntArray;

// FIXME(bnoordhuis) Make these per-context?
QUEUE handle_wrap_queue = { &handle_wrap_queue, &handle_wrap_queue };
QUEUE req_wrap_queue = { &req_wrap_queue, &req_wrap_queue };

static bool print_eval = false;
static bool force_repl = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static const char* eval_string = NULL;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static int debug_port = 5858;
static bool v8_is_profiling = false;

// used by C++ modules as well
bool no_deprecation = false;

// process-relative uptime base, initialized at start-up
static double prog_start_time;
static bool debugger_running;
static uv_async_t dispatch_debug_messages_async;

// Declared in node_internals.h
Isolate* node_isolate = NULL;


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
  return new char[length];
}


void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  if (length > kMaxLength)
    return NULL;
  return new char[length];
}


void ArrayBufferAllocator::Free(void* data, size_t length) {
  delete[] static_cast<char*>(data);
}


static void CheckImmediate(uv_check_t* handle, int status) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  Context::Scope context_scope(env->context());
  MakeCallback(env, env->process_object(), env->immediate_callback_string());
}


static void IdleImmediateDummy(uv_idle_t*, int) {
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


Local<Value> ErrnoException(int errorno,
                            const char *syscall,
                            const char *msg,
                            const char *path) {
  Environment* env = Environment::GetCurrent(node_isolate);

  Local<Value> e;
  Local<String> estring = OneByteString(node_isolate, errno_string(errorno));
  if (msg == NULL || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(node_isolate, msg);

  Local<String> cons1 =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(node_isolate, ", "));
  Local<String> cons2 = String::Concat(cons1, message);

  if (path) {
    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(node_isolate, " '"));
    Local<String> cons4 =
        String::Concat(cons3, String::NewFromUtf8(node_isolate, path));
    Local<String> cons5 =
        String::Concat(cons4, FIXED_ONE_BYTE_STRING(node_isolate, "'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();
  obj->Set(env->errno_string(), Integer::New(errorno, node_isolate));
  obj->Set(env->code_string(), estring);

  if (path != NULL) {
    obj->Set(env->path_string(), String::NewFromUtf8(node_isolate, path));
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(node_isolate, syscall));
  }

  return e;
}


// hack alert! copy of ErrnoException, tuned for uv errors
Local<Value> UVException(int errorno,
                         const char *syscall,
                         const char *msg,
                         const char *path) {
  Environment* env = Environment::GetCurrent(node_isolate);

  if (!msg || !msg[0])
    msg = uv_strerror(errorno);

  Local<String> estring = OneByteString(node_isolate, uv_err_name(errorno));
  Local<String> message = OneByteString(node_isolate, msg);
  Local<String> cons1 =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(node_isolate, ", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e;

  Local<String> path_str;

  if (path) {
#ifdef _WIN32
    if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
      path_str = String::Concat(FIXED_ONE_BYTE_STRING(node_isolate, "\\\\"),
                                String::NewFromUtf8(node_isolate, path + 8));
    } else if (strncmp(path, "\\\\?\\", 4) == 0) {
      path_str = String::NewFromUtf8(node_isolate, path + 4);
    } else {
      path_str = String::NewFromUtf8(node_isolate, path);
    }
#else
    path_str = String::NewFromUtf8(node_isolate, path);
#endif

    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(node_isolate, " '"));
    Local<String> cons4 =
        String::Concat(cons3, path_str);
    Local<String> cons5 =
        String::Concat(cons4, FIXED_ONE_BYTE_STRING(node_isolate, "'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();
  // TODO(piscisaureus) errno should probably go
  obj->Set(env->errno_string(), Integer::New(errorno, node_isolate));
  obj->Set(env->code_string(), estring);

  if (path != NULL) {
    obj->Set(env->path_string(), path_str);
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(node_isolate, syscall));
  }

  return e;
}


#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char *winapi_strerror(const int errorno) {
  char *errmsg = NULL;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, NULL);

  if (errmsg) {
    // Remove trailing newlines
    for (int i = strlen(errmsg) - 1;
        i >= 0 && (errmsg[i] == '\n' || errmsg[i] == '\r'); i--) {
      errmsg[i] = '\0';
    }

    return errmsg;
  } else {
    // FormatMessage failed
    return "Unknown error";
  }
}


Local<Value> WinapiErrnoException(int errorno,
                                  const char* syscall,
                                  const char* msg,
                                  const char* path) {
  Environment* env = Environment::GetCurrent(node_isolate);

  Local<Value> e;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno);
  }
  Local<String> message = OneByteString(node_isolate, msg);

  if (path) {
    Local<String> cons1 =
        String::Concat(message, FIXED_ONE_BYTE_STRING(node_isolate, " '"));
    Local<String> cons2 =
        String::Concat(cons1, String::NewFromUtf8(node_isolate, path));
    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(node_isolate, "'"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Object> obj = e->ToObject();
  obj->Set(env->errno_string(), Integer::New(errorno, node_isolate));

  if (path != NULL) {
    obj->Set(env->path_string(), String::NewFromUtf8(node_isolate, path));
  }

  if (syscall != NULL) {
    obj->Set(env->syscall_string(), OneByteString(node_isolate, syscall));
  }

  return e;
}
#endif


void SetupAsyncListener(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  assert(args[0]->IsObject());
  assert(args[1]->IsFunction());
  assert(args[2]->IsFunction());
  assert(args[3]->IsFunction());
  assert(args[4]->IsFunction());
  assert(args[5]->IsFunction());

  env->set_async_listener_run_function(args[1].As<Function>());
  env->set_async_listener_load_function(args[2].As<Function>());
  env->set_async_listener_unload_function(args[3].As<Function>());
  env->set_async_listener_push_function(args[4].As<Function>());
  env->set_async_listener_strip_function(args[5].As<Function>());

  Local<Object> async_listener_flag_obj = args[0].As<Object>();
  Environment::AsyncListener* async_listener = env->async_listener();
  async_listener_flag_obj->SetIndexedPropertiesToExternalArrayData(
      async_listener->fields(),
      kExternalUnsignedIntArray,
      async_listener->fields_count());

  // Do a little housekeeping.
  env->process_object()->Delete(
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupAsyncListener"));
}


void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  assert(args[0]->IsObject() && args[1]->IsFunction());

  // Values use to cross communicate with processNextTick.
  Local<Object> tick_info_obj = args[0].As<Object>();
  tick_info_obj->SetIndexedPropertiesToExternalArrayData(
      env->tick_info()->fields(),
      kExternalUnsignedIntArray,
      env->tick_info()->fields_count());

  env->set_tick_callback_function(args[1].As<Function>());

  // Do a little housekeeping.
  env->process_object()->Delete(
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "_setupNextTick"));
}


Handle<Value> MakeCallback(Environment* env,
                           Handle<Object> object,
                           const Handle<Function> callback,
                           int argc,
                           Handle<Value> argv[]) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  assert(env->context() == env->isolate()->GetCurrentContext());

  Local<Object> process = env->process_object();

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  // TODO(trevnorris): This is sucky for performance. Fix it.
  bool has_async_queue = object->Has(env->async_queue_string());
  if (has_async_queue) {
    Local<Value> argv[] = { object };
    env->async_listener_load_function()->Call(process, ARRAY_SIZE(argv), argv);

    if (try_catch.HasCaught())
      return Undefined(node_isolate);
  }

  Local<Value> ret = callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(node_isolate);
  }

  if (has_async_queue) {
    Local<Value> val = object.As<Value>();
    env->async_listener_unload_function()->Call(process, 1, &val);

    if (try_catch.HasCaught())
      return Undefined(node_isolate);
  }

  Environment::TickInfo* tick_info = env->tick_info();

  if (tick_info->in_tick()) {
    return ret;
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
    return Undefined(node_isolate);
  }

  return ret;
}


// Internal only.
Handle<Value> MakeCallback(Environment* env,
                           const Handle<Object> object,
                           uint32_t index,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Function> callback = object->Get(index).As<Function>();
  assert(callback->IsFunction());

  return MakeCallback(env, object, callback, argc, argv);
}


Handle<Value> MakeCallback(Environment* env,
                           const Handle<Object> object,
                           const Handle<String> symbol,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Function> callback = object->Get(symbol).As<Function>();
  assert(callback->IsFunction());
  return MakeCallback(env, object, callback, argc, argv);
}


Handle<Value> MakeCallback(Environment* env,
                           const Handle<Object> object,
                           const char* method,
                           int argc,
                           Handle<Value> argv[]) {
  Local<String> method_string = OneByteString(node_isolate, method);
  return MakeCallback(env, object, method_string, argc, argv);
}


Handle<Value> MakeCallback(const Handle<Object> object,
                           const char* method,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Context> context = object->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  HandleScope handle_scope(env->isolate());
  return handle_scope.Close(MakeCallback(env, object, method, argc, argv));
}


Handle<Value> MakeCallback(const Handle<Object> object,
                           const Handle<String> symbol,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Context> context = object->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  HandleScope handle_scope(env->isolate());
  return handle_scope.Close(MakeCallback(env, object, symbol, argc, argv));
}


Handle<Value> MakeCallback(const Handle<Object> object,
                           const Handle<Function> callback,
                           int argc,
                           Handle<Value> argv[]) {
  Local<Context> context = object->CreationContext();
  Environment* env = Environment::GetCurrent(context);
  Context::Scope context_scope(context);
  HandleScope handle_scope(env->isolate());
  return handle_scope.Close(MakeCallback(env, object, callback, argc, argv));
}


enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope(node_isolate);

  if (!encoding_v->IsString())
    return _default;

  String::Utf8Value encoding(encoding_v);

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

Local<Value> Encode(const void *buf, size_t len, enum encoding encoding) {
  return StringBytes::Encode(static_cast<const char*>(buf),
                             len,
                             encoding);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(v8::Handle<v8::Value> val, enum encoding encoding) {
  HandleScope scope(node_isolate);

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  return StringBytes::Size(val, encoding);
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t DecodeWrite(char *buf,
                    size_t buflen,
                    v8::Handle<v8::Value> val,
                    enum encoding encoding) {
  return StringBytes::Write(buf, buflen, val, encoding, NULL);
}

void DisplayExceptionLine(Handle<Message> message) {
  // Prevent re-entry into this function.  For example, if there is
  // a throw from a program in vm.runInThisContext(code, filename, true),
  // then we want to show the original failure, not the secondary one.
  static bool displayed_error = false;

  if (displayed_error)
    return;
  displayed_error = true;

  uv_tty_reset_mode();

  fprintf(stderr, "\n");

  if (!message.IsEmpty()) {
    // Print (filename):(line number): (message).
    String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = *filename;
    int linenum = message->GetLineNumber();
    fprintf(stderr, "%s:%i\n", filename_string, linenum);
    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
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

    fprintf(stderr, "%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    for (int i = 0; i < start; i++) {
      fputc((sourceline_string[i] == '\t') ? '\t' : ' ', stderr);
    }
    for (int i = start; i < end; i++) {
      fputc('^', stderr);
    }
    fputc('\n', stderr);
  }
}


static void ReportException(Handle<Value> er, Handle<Message> message) {
  HandleScope scope(node_isolate);

  DisplayExceptionLine(message);

  Local<Value> trace_value(
      er->ToObject()->Get(FIXED_ONE_BYTE_STRING(node_isolate, "stack")));
  String::Utf8Value trace(trace_value);

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
      message = err_obj->Get(FIXED_ONE_BYTE_STRING(node_isolate, "message"));
      name = err_obj->Get(FIXED_ONE_BYTE_STRING(node_isolate, "name"));
    }

    if (message.IsEmpty() ||
        message->IsUndefined() ||
        name.IsEmpty() ||
        name->IsUndefined()) {
      // Not an error object. Just print as-is.
      String::Utf8Value message(er);
      fprintf(stderr, "%s\n", *message);
    } else {
      String::Utf8Value name_string(name);
      String::Utf8Value message_string(message);
      fprintf(stderr, "%s: %s\n", *name_string, *message_string);
    }
  }

  fflush(stderr);
}


static void ReportException(const TryCatch& try_catch) {
  ReportException(try_catch.Exception(), try_catch.Message());
}


// Executes a str within the current v8 context.
Local<Value> ExecuteString(Handle<String> source, Handle<Value> filename) {
  HandleScope scope(node_isolate);
  TryCatch try_catch;

  // try_catch must be nonverbose to disable FatalException() handler,
  // we will handle exceptions ourself.
  try_catch.SetVerbose(false);

  Local<v8::Script> script = v8::Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(try_catch);
    exit(3);
  }

  Local<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(try_catch);
    exit(4);
  }

  return scope.Close(result);
}


static void GetActiveRequests(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Local<Array> ary = Array::New();
  QUEUE* q = NULL;
  int i = 0;

  QUEUE_FOREACH(q, &req_wrap_queue) {
    ReqWrap<uv_req_t>* w = container_of(q, ReqWrap<uv_req_t>, req_wrap_queue_);
    if (w->persistent().IsEmpty())
      continue;
    ary->Set(i++, w->object());
  }

  args.GetReturnValue().Set(ary);
}


// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
void GetActiveHandles(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Local<Array> ary = Array::New();
  QUEUE* q = NULL;
  int i = 0;

  Local<String> owner_sym = FIXED_ONE_BYTE_STRING(node_isolate, "owner");

  QUEUE_FOREACH(q, &handle_wrap_queue) {
    HandleWrap* w = container_of(q, HandleWrap, handle_wrap_queue_);
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
  HandleScope scope(node_isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowError("Bad argument.");  // FIXME(bnoordhuis) ThrowTypeError?
  }

  String::Utf8Value path(args[0]);
  int err = uv_chdir(*path);
  if (err) {
    return ThrowUVException(err, "uv_chdir");
  }
}


static void Cwd(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
#ifdef _WIN32
  /* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
  char buf[MAX_PATH * 4 + 1];
#else
  char buf[PATH_MAX + 1];
#endif

  int err = uv_cwd(buf, ARRAY_SIZE(buf) - 1);
  if (err) {
    return ThrowUVException(err, "uv_cwd");
  }

  buf[ARRAY_SIZE(buf) - 1] = '\0';
  Local<String> cwd = String::NewFromUtf8(node_isolate, buf);

  args.GetReturnValue().Set(cwd);
}


static void Umask(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  uint32_t old;

  if (args.Length() < 1 || args[0]->IsUndefined()) {
    old = umask(0);
    umask(static_cast<mode_t>(old));
  } else if (!args[0]->IsInt32() && !args[0]->IsString()) {
    return ThrowTypeError("argument must be an integer or octal string.");
  } else {
    int oct;
    if (args[0]->IsInt32()) {
      oct = args[0]->Uint32Value();
    } else {
      oct = 0;
      String::Utf8Value str(args[0]);

      // Parse the octal string.
      for (int i = 0; i < str.length(); i++) {
        char c = (*str)[i];
        if (c > '7' || c < '0') {
          return ThrowTypeError("invalid octal string");
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
    String::Utf8Value name(value);
    return uid_by_name(*name);
  }
}


static gid_t gid_by_name(Handle<Value> value) {
  if (value->IsUint32()) {
    return static_cast<gid_t>(value->Uint32Value());
  } else {
    String::Utf8Value name(value);
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
  HandleScope scope(node_isolate);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return ThrowTypeError("setgid argument must be a number or a string");
  }

  gid_t gid = gid_by_name(args[0]);

  if (gid == gid_not_found) {
    return ThrowError("setgid group id does not exist");
  }

  if (setgid(gid)) {
    return ThrowErrnoException(errno, "setgid");
  }
}


static void SetUid(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return ThrowTypeError("setuid argument must be a number or a string");
  }

  uid_t uid = uid_by_name(args[0]);

  if (uid == uid_not_found) {
    return ThrowError("setuid user id does not exist");
  }

  if (setuid(uid)) {
    return ThrowErrnoException(errno, "setuid");
  }
}


static void GetGroups(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int ngroups = getgroups(0, NULL);

  if (ngroups == -1) {
    return ThrowErrnoException(errno, "getgroups");
  }

  gid_t* groups = new gid_t[ngroups];

  ngroups = getgroups(ngroups, groups);

  if (ngroups == -1) {
    delete[] groups;
    return ThrowErrnoException(errno, "getgroups");
  }

  Local<Array> groups_list = Array::New(ngroups);
  bool seen_egid = false;
  gid_t egid = getegid();

  for (int i = 0; i < ngroups; i++) {
    groups_list->Set(i, Integer::New(groups[i], node_isolate));
    if (groups[i] == egid)
      seen_egid = true;
  }

  delete[] groups;

  if (seen_egid == false) {
    groups_list->Set(ngroups, Integer::New(egid, node_isolate));
  }

  args.GetReturnValue().Set(groups_list);
}


static void SetGroups(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsArray()) {
    return ThrowTypeError("argument 1 must be an array");
  }

  Local<Array> groups_list = args[0].As<Array>();
  size_t size = groups_list->Length();
  gid_t* groups = new gid_t[size];

  for (size_t i = 0; i < size; i++) {
    gid_t gid = gid_by_name(groups_list->Get(i));

    if (gid == gid_not_found) {
      delete[] groups;
      return ThrowError("group name not found");
    }

    groups[i] = gid;
  }

  int rc = setgroups(size, groups);
  delete[] groups;

  if (rc == -1) {
    return ThrowErrnoException(errno, "setgroups");
  }
}


static void InitGroups(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return ThrowTypeError("argument 1 must be a number or a string");
  }

  if (!args[1]->IsUint32() && !args[1]->IsString()) {
    return ThrowTypeError("argument 2 must be a number or a string");
  }

  String::Utf8Value arg0(args[0]);
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
    return ThrowError("initgroups user not found");
  }

  extra_group = gid_by_name(args[1]);

  if (extra_group == gid_not_found) {
    if (must_free)
      free(user);
    return ThrowError("initgroups extra group not found");
  }

  int rc = initgroups(user, extra_group);

  if (must_free) {
    free(user);
  }

  if (rc) {
    return ThrowErrnoException(errno, "initgroups");
  }
}

#endif  // __POSIX__ && !defined(__ANDROID__)


void Exit(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  exit(args[0]->IntegerValue());
}


static void Uptime(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  double uptime;
  if (uv_uptime(&uptime))
    return;
  args.GetReturnValue().Set(uptime - prog_start_time);
}


void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  size_t rss;
  int err = uv_resident_set_memory(&rss);
  if (err) {
    return ThrowUVException(err, "uv_resident_set_memory");
  }

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  node_isolate->GetHeapStatistics(&v8_heap_stats);

  Local<Integer> heap_total =
      Integer::NewFromUnsigned(v8_heap_stats.total_heap_size(), node_isolate);
  Local<Integer> heap_used =
      Integer::NewFromUnsigned(v8_heap_stats.used_heap_size(), node_isolate);

  Local<Object> info = Object::New();
  info->Set(env->rss_string(), Number::New(node_isolate, rss));
  info->Set(env->heap_total_string(), heap_total);
  info->Set(env->heap_used_string(), heap_used);

  args.GetReturnValue().Set(info);
}


void Kill(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() != 2) {
    return ThrowError("Bad argument.");
  }

  int pid = args[0]->IntegerValue();
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
  HandleScope scope(node_isolate);

  uint64_t t = uv_hrtime();

  if (args.Length() > 0) {
    // return a time diff tuple
    if (!args[0]->IsArray()) {
      return ThrowTypeError("process.hrtime() only accepts an Array tuple.");
    }
    Local<Array> inArray = Local<Array>::Cast(args[0]);
    uint64_t seconds = inArray->Get(0)->Uint32Value();
    uint64_t nanos = inArray->Get(1)->Uint32Value();
    t -= (seconds * NANOS_PER_SEC) + nanos;
  }

  Local<Array> tuple = Array::New(2);
  tuple->Set(0, Integer::NewFromUnsigned(t / NANOS_PER_SEC, node_isolate));
  tuple->Set(1, Integer::NewFromUnsigned(t % NANOS_PER_SEC, node_isolate));
  args.GetReturnValue().Set(tuple);
}


typedef void (UV_DYNAMIC* extInit)(Handle<Object> exports);

// DLOpen is process.dlopen(module, filename).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  char symbol[1024], *base, *pos;
  uv_lib_t lib;
  int r;

  if (args.Length() < 2) {
    return ThrowError("process.dlopen takes exactly 2 arguments.");
  }

  Local<Object> module = args[0]->ToObject();  // Cast
  String::Utf8Value filename(args[1]);  // Cast

  Local<String> exports_string = env->exports_string();
  Local<Object> exports = module->Get(exports_string)->ToObject();

  if (uv_dlopen(*filename, &lib)) {
    Local<String> errmsg = OneByteString(env->isolate(), uv_dlerror(&lib));
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, args[1]->ToString());
#endif  // _WIN32
    ThrowException(Exception::Error(errmsg));
    return;
  }

  String::Utf8Value path(args[1]);
  base = *path;

  /* Find the shared library filename within the full path. */
#ifdef __POSIX__
  pos = strrchr(base, '/');
  if (pos != NULL) {
    base = pos + 1;
  }
#else  // Windows
  for (;;) {
    pos = strpbrk(base, "\\/:");
    if (pos == NULL) {
      break;
    }
    base = pos + 1;
  }
#endif  // __POSIX__

  /* Strip the .node extension. */
  pos = strrchr(base, '.');
  if (pos != NULL) {
    *pos = '\0';
  }

  /* Add the `_module` suffix to the extension name. */
  r = snprintf(symbol, sizeof symbol, "%s_module", base);
  if (r <= 0 || static_cast<size_t>(r) >= sizeof symbol) {
    return ThrowError("Out of memory.");
  }

  /* Replace dashes with underscores. When loading foo-bar.node,
   * look for foo_bar_module, not foo-bar_module.
   */
  for (pos = symbol; *pos != '\0'; ++pos) {
    if (*pos == '-')
      *pos = '_';
  }

  node_module_struct *mod;
  if (uv_dlsym(&lib, symbol, reinterpret_cast<void**>(&mod))) {
    char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "Symbol %s not found.", symbol);
    return ThrowError(errmsg);
  }

  if (mod->version != NODE_MODULE_VERSION) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "Module version mismatch. Expected %d, got %d.",
             NODE_MODULE_VERSION, mod->version);
    return ThrowError(errmsg);
  }

  // Execute the C++ module
  if (mod->register_context_func != NULL) {
    mod->register_context_func(exports, module, env->context());
  } else if (mod->register_func != NULL) {
    mod->register_func(exports, module);
  } else {
    return ThrowError("Module has no declared entry point.");
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
#if defined(DEBUG)
  abort();
#endif
  exit(5);
}


NO_RETURN void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to supress compiler warning
  abort();
}


void FatalException(Handle<Value> error, Handle<Message> message) {
  HandleScope scope(node_isolate);

  Environment* env = Environment::GetCurrent(node_isolate);
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Function> fatal_exception_function =
      process_object->Get(fatal_exception_string).As<Function>();

  if (!fatal_exception_function->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(error, message);
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
    ReportException(fatal_try_catch);
    exit(7);
  }

  if (false == caught->BooleanValue()) {
    ReportException(error, message);
    exit(1);
  }
}


void FatalException(const TryCatch& try_catch) {
  HandleScope scope(node_isolate);
  // TODO(bajtos) do not call FatalException if try_catch is verbose
  // (requires V8 API to expose getter for try_catch.is_verbose_)
  FatalException(try_catch.Exception(), try_catch.Message());
}


void OnMessage(Handle<Message> message, Handle<Value> error) {
  // The current version of V8 sends messages for errors only
  // (thus `error` is always set).
  FatalException(error, message);
}


static void Binding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  Local<String> module = args[0]->ToString();
  String::Utf8Value module_v(module);

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
  modules->Set(l, OneByteString(node_isolate, buf));

  node_module_struct* mod = get_builtin_module(*module_v);
  if (mod != NULL) {
    exports = Object::New();
    // Internal bindings don't have a "module" object, only exports.
    assert(mod->register_func == NULL);
    assert(mod->register_context_func != NULL);
    Local<Value> unused = Undefined(env->isolate());
    mod->register_context_func(exports, unused, env->context());
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New();
    DefineConstants(exports);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "natives")) {
    exports = Object::New();
    DefineJavaScript(exports);
    cache->Set(module, exports);
  } else {
    return ThrowError("No such module");
  }

  args.GetReturnValue().Set(exports);
}


static void ProcessTitleGetter(Local<String> property,
                               const PropertyCallbackInfo<Value>& info) {
  HandleScope scope(node_isolate);
  char buffer[512];
  uv_get_process_title(buffer, sizeof(buffer));
  info.GetReturnValue().Set(String::NewFromUtf8(node_isolate, buffer));
}


static void ProcessTitleSetter(Local<String> property,
                               Local<Value> value,
                               const PropertyCallbackInfo<void>& info) {
  HandleScope scope(node_isolate);
  String::Utf8Value title(value);
  // TODO(piscisaureus): protect with a lock
  uv_set_process_title(*title);
}


static void EnvGetter(Local<String> property,
                      const PropertyCallbackInfo<Value>& info) {
  HandleScope scope(node_isolate);
#ifdef __POSIX__
  String::Utf8Value key(property);
  const char* val = getenv(*key);
  if (val) {
    return info.GetReturnValue().Set(String::NewFromUtf8(node_isolate, val));
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
    Local<String> rc = String::NewFromTwoByte(node_isolate, two_byte_buffer);
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
  HandleScope scope(node_isolate);
#ifdef __POSIX__
  String::Utf8Value key(property);
  String::Utf8Value val(value);
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
  HandleScope scope(node_isolate);
  int32_t rc = -1;  // Not found unless proven otherwise.
#ifdef __POSIX__
  String::Utf8Value key(property);
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
  HandleScope scope(node_isolate);
  bool rc = true;
#ifdef __POSIX__
  String::Utf8Value key(property);
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
  HandleScope scope(node_isolate);
#ifdef __POSIX__
  int size = 0;
  while (environ[size])
    size++;

  Local<Array> env = Array::New(size);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    Local<String> name = String::NewFromUtf8(node_isolate,
                                             var,
                                             String::kNormalString,
                                             length);
    env->Set(i, name);
  }
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == NULL)
    return;  // This should not happen.
  Local<Array> env = Array::New();
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
    Local<String> value = String::NewFromTwoByte(node_isolate,
                                                 two_byte_buffer,
                                                 String::kNormalString,
                                                 two_byte_buffer_len);
    env->Set(i++, value);
    p = s + wcslen(s) + 1;
  }
  FreeEnvironmentStringsW(environment);
#endif

  info.GetReturnValue().Set(env);
}


static Handle<Object> GetFeatures() {
  HandleScope scope(node_isolate);

  Local<Object> obj = Object::New();
#if defined(DEBUG) && DEBUG
  Local<Value> debug = True(node_isolate);
#else
  Local<Value> debug = False(node_isolate);
#endif  // defined(DEBUG) && DEBUG

  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "debug"), debug);

  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "uv"), True(node_isolate));
  // TODO(bnoordhuis) ping libuv
  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "ipv6"), True(node_isolate));

#ifdef OPENSSL_NPN_NEGOTIATED
  Local<Boolean> tls_npn = True(node_isolate);
#else
  Local<Boolean> tls_npn = False(node_isolate);
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "tls_npn"), tls_npn);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  Local<Boolean> tls_sni = True(node_isolate);
#else
  Local<Boolean> tls_sni = False(node_isolate);
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "tls_sni"), tls_sni);

  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "tls"),
           Boolean::New(get_builtin_module("crypto") != NULL));

  return scope.Close(obj);
}


static void DebugPortGetter(Local<String> property,
                            const PropertyCallbackInfo<Value>& info) {
  HandleScope scope(node_isolate);
  info.GetReturnValue().Set(debug_port);
}


static void DebugPortSetter(Local<String> property,
                            Local<Value> value,
                            const PropertyCallbackInfo<void>& info) {
  HandleScope scope(node_isolate);
  debug_port = value->NumberValue();
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args);
static void DebugPause(const FunctionCallbackInfo<Value>& args);
static void DebugEnd(const FunctionCallbackInfo<Value>& args);


void NeedImmediateCallbackGetter(Local<String> property,
                                 const PropertyCallbackInfo<Value>& info) {
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
  Environment* env = Environment::GetCurrent(info.GetIsolate());
  HandleScope handle_scope(info.GetIsolate());

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


void SetIdle(uv_prepare_t* handle, int) {
  Environment* env = Environment::from_idle_prepare_handle(handle);
  env->isolate()->GetCpuProfiler()->SetIdle(true);
}


void ClearIdle(uv_check_t* handle, int) {
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
  StartProfilerIdleNotifier(Environment::GetCurrent(args.GetIsolate()));
}


void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  StopProfilerIdleNotifier(Environment::GetCurrent(args.GetIsolate()));
}


#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->Set(OneByteString(node_isolate, str), var, v8::ReadOnly);            \
  } while (0)


void SetupProcessObject(Environment* env,
                        int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv) {
  HandleScope scope(node_isolate);

  Local<Object> process = env->process_object();

  process->SetAccessor(FIXED_ONE_BYTE_STRING(node_isolate, "title"),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

  // process.version
  READONLY_PROPERTY(process,
                    "version",
                    FIXED_ONE_BYTE_STRING(node_isolate, NODE_VERSION));

  // process.moduleLoadList
  READONLY_PROPERTY(process,
                    "moduleLoadList",
                    env->module_load_list_array());

  // process.versions
  Local<Object> versions = Object::New();
  READONLY_PROPERTY(process, "versions", versions);

  const char http_parser_version[] = NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
                                     "."
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR);
  READONLY_PROPERTY(versions,
                    "http_parser",
                    FIXED_ONE_BYTE_STRING(node_isolate, http_parser_version));

  // +1 to get rid of the leading 'v'
  READONLY_PROPERTY(versions,
                    "node",
                    OneByteString(node_isolate, NODE_VERSION + 1));
  READONLY_PROPERTY(versions,
                    "v8",
                    OneByteString(node_isolate, V8::GetVersion()));
  READONLY_PROPERTY(versions,
                    "uv",
                    OneByteString(node_isolate, uv_version_string()));
  READONLY_PROPERTY(versions,
                    "zlib",
                    FIXED_ONE_BYTE_STRING(node_isolate, ZLIB_VERSION));

  const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);
  READONLY_PROPERTY(versions,
                    "modules",
                    FIXED_ONE_BYTE_STRING(node_isolate, node_modules_version));

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
        OneByteString(node_isolate, &OPENSSL_VERSION_TEXT[i], j - i));
  }
#endif

  // process.arch
  READONLY_PROPERTY(process, "arch", OneByteString(node_isolate, ARCH));

  // process.platform
  READONLY_PROPERTY(process,
                    "platform",
                    OneByteString(node_isolate, PLATFORM));

  // process.argv
  Local<Array> arguments = Array::New(argc);
  for (int i = 0; i < argc; ++i) {
    arguments->Set(i, String::NewFromUtf8(node_isolate, argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "argv"), arguments);

  // process.execArgv
  Local<Array> exec_arguments = Array::New(exec_argc);
  for (int i = 0; i < exec_argc; ++i) {
    exec_arguments->Set(i, String::NewFromUtf8(node_isolate, exec_argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "execArgv"), exec_arguments);

  // create process.env
  Local<ObjectTemplate> process_env_template = ObjectTemplate::New();
  process_env_template->SetNamedPropertyHandler(EnvGetter,
                                                EnvSetter,
                                                EnvQuery,
                                                EnvDeleter,
                                                EnvEnumerator,
                                                Object::New());
  Local<Object> process_env = process_env_template->NewInstance();
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "env"), process_env);

  READONLY_PROPERTY(process, "pid", Integer::New(getpid(), node_isolate));
  READONLY_PROPERTY(process, "features", GetFeatures());
  process->SetAccessor(
      FIXED_ONE_BYTE_STRING(node_isolate, "_needImmediateCallback"),
      NeedImmediateCallbackGetter,
      NeedImmediateCallbackSetter);

  // -e, --eval
  if (eval_string) {
    READONLY_PROPERTY(process,
                      "_eval",
                      String::NewFromUtf8(node_isolate, eval_string));
  }

  // -p, --print
  if (print_eval) {
    READONLY_PROPERTY(process, "_print_eval", True(node_isolate));
  }

  // -i, --interactive
  if (force_repl) {
    READONLY_PROPERTY(process, "_forceRepl", True(node_isolate));
  }

  // --no-deprecation
  if (no_deprecation) {
    READONLY_PROPERTY(process, "noDeprecation", True(node_isolate));
  }

  // --throw-deprecation
  if (throw_deprecation) {
    READONLY_PROPERTY(process, "throwDeprecation", True(node_isolate));
  }

  // --trace-deprecation
  if (trace_deprecation) {
    READONLY_PROPERTY(process, "traceDeprecation", True(node_isolate));
  }

  size_t exec_path_len = 2 * PATH_MAX;
  char* exec_path = new char[exec_path_len];
  Local<String> exec_path_value;
  if (uv_exepath(exec_path, &exec_path_len) == 0) {
    exec_path_value = String::NewFromUtf8(node_isolate,
                                          exec_path,
                                          String::kNormalString,
                                          exec_path_len);
  } else {
    exec_path_value = String::NewFromUtf8(node_isolate, argv[0]);
  }
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "execPath"),
               exec_path_value);
  delete[] exec_path;

  process->SetAccessor(FIXED_ONE_BYTE_STRING(node_isolate, "debugPort"),
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

  NODE_SET_METHOD(process, "_setupAsyncListener", SetupAsyncListener);
  NODE_SET_METHOD(process, "_setupNextTick", SetupNextTick);

  // values use to cross communicate with processNextTick
  Local<Object> tick_info_obj = Object::New();
  tick_info_obj->SetIndexedPropertiesToExternalArrayData(
      env->tick_info()->fields(),
      kExternalUnsignedIntArray,
      env->tick_info()->fields_count());
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_tickInfo"), tick_info_obj);

  // pre-set _events object for faster emit checks
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_events"), Object::New());
}


#undef READONLY_PROPERTY


static void AtExit() {
  uv_tty_reset_mode();
}


static void SignalExit(int signal) {
  uv_tty_reset_mode();
  _exit(128 + signal);
}


// Most of the time, it's best to use `console.error` to write
// to the process.stderr stream.  However, in some cases, such as
// when debugging the stream.Writable class or the process.nextTick
// function, it is useful to bypass JavaScript entirely.
static void RawDebug(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  assert(args.Length() == 1 && args[0]->IsString() &&
         "must be called with a single string");

  String::Utf8Value message(args[0]);
  fprintf(stderr, "%s\n", *message);
  fflush(stderr);
}


void Load(Environment* env) {
  HandleScope handle_scope(node_isolate);

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

  Local<String> script_name = FIXED_ONE_BYTE_STRING(node_isolate, "node.js");
  Local<Value> f_value = ExecuteString(MainSource(), script_name);
  if (try_catch.HasCaught())  {
    ReportException(try_catch);
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
  InitDTrace(global);
#endif

#if defined HAVE_PERFCTR
  InitPerfCounters(global);
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

static void ParseDebugOpt(const char* arg) {
  const char *p = 0;

  if (strstr(arg, "--debug-port=") == arg) {
    p = 1 + strchr(arg, '=');
    debug_port = atoi(p);
  } else {
    use_debug_agent = true;
    if (!strcmp(arg, "--debug-brk")) {
      debug_wait_connect = true;
      return;
    } else if (!strcmp(arg, "--debug")) {
      return;
    } else if (strstr(arg, "--debug-brk=") == arg) {
      debug_wait_connect = true;
      p = 1 + strchr(arg, '=');
      debug_port = atoi(p);
    } else if (strstr(arg, "--debug=") == arg) {
      p = 1 + strchr(arg, '=');
      debug_port = atoi(p);
    }
  }
  if (p && debug_port > 1024 && debug_port <  65536)
      return;

  fprintf(stderr, "Bad debug option.\n");
  if (p)
    fprintf(stderr, "Debug port must be in range 1025 to 65535.\n");

  PrintHelp();
  exit(12);
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
         "  --trace-deprecation  show stack traces on deprecations\n"
         "  --v8-options         print v8 command line options\n"
         "  --max-stack-size=val set max v8 stack size (bytes)\n"
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

    if (strstr(arg, "--debug") == arg) {
      ParseDebugOpt(arg);
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
      } else if (argv[index + 1] != NULL && argv[index + 1][0] != '-') {
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
static void DispatchMessagesDebugAgentCallback() {
  uv_async_send(&dispatch_debug_messages_async);
}


// Called from the main thread.
static void EnableDebug(bool wait_connect) {
  assert(debugger_running == false);
  Isolate* isolate = node_isolate;  // TODO(bnoordhuis) Multi-isolate support.
  Isolate::Scope isolate_scope(isolate);
  v8::Debug::SetDebugMessageDispatchHandler(DispatchMessagesDebugAgentCallback,
                                            false);
  debugger_running = v8::Debug::EnableAgent("node " NODE_VERSION,
                                            debug_port,
                                            wait_connect);
  if (debugger_running == false) {
    fprintf(stderr, "Starting debugger on port %d failed\n", debug_port);
    fflush(stderr);
    return;
  }
  fprintf(stderr, "Debugger listening on port %d\n", debug_port);
  fflush(stderr);

  Environment* env = Environment::GetCurrentChecked(isolate);
  if (env == NULL)
    return;  // Still starting up.

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Object> message = Object::New();
  message->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "cmd"),
               FIXED_ONE_BYTE_STRING(env->isolate(), "NODE_DEBUG_ENABLED"));
  Local<Value> argv[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "internalMessage"),
    message
  };
  MakeCallback(env, env->process_object(), "emit", ARRAY_SIZE(argv), argv);
}


// Called from the main thread.
static void DispatchDebugMessagesAsyncCallback(uv_async_t* handle, int status) {
  if (debugger_running == false) {
    fprintf(stderr, "Starting debugger agent.\n");
    EnableDebug(false);
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


static void RegisterSignalHandler(int signal, void (*handler)(int signal)) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  sigaction(signal, &sa, NULL);
}


void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() != 1) {
    return ThrowError("Invalid number of arguments.");
  }

  pid_t pid;
  int r;

  pid = args[0]->IntegerValue();
  r = kill(pid, SIGUSR1);
  if (r != 0) {
    return ThrowErrnoException(errno, "kill");
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
  HandleScope scope(node_isolate);
  DWORD pid;
  HANDLE process = NULL;
  HANDLE thread = NULL;
  HANDLE mapping = NULL;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = NULL;

  if (args.Length() != 1) {
    ThrowError("Invalid number of arguments.");
    goto out;
  }

  pid = (DWORD) args[0]->IntegerValue();

  process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                            PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                            PROCESS_VM_READ,
                        FALSE,
                        pid);
  if (process == NULL) {
    ThrowException(WinapiErrnoException(GetLastError(), "OpenProcess"));
    goto out;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       ARRAY_SIZE(mapping_name)) < 0) {
    ThrowErrnoException(errno, "sprintf");
    goto out;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == NULL) {
    ThrowException(WinapiErrnoException(GetLastError(), "OpenFileMappingW"));
    goto out;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping,
                    FILE_MAP_READ,
                    0,
                    0,
                    sizeof *handler));
  if (handler == NULL || *handler == NULL) {
    ThrowException(WinapiErrnoException(GetLastError(), "MapViewOfFile"));
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
    ThrowException(WinapiErrnoException(GetLastError(), "CreateRemoteThread"));
    goto out;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    ThrowException(WinapiErrnoException(GetLastError(), "WaitForSingleObject"));
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
  v8::Debug::DebugBreak(node_isolate);
}


static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
  if (debugger_running) {
    v8::Debug::DisableAgent();
    debugger_running = false;
  }
}


void Init(int* argc,
          const char** argv,
          int* exec_argc,
          const char*** exec_argv) {
  // Initialize prog_start_time to get relative uptime.
  uv_uptime(&prog_start_time);

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // init async debug messages dispatching
  // FIXME(bnoordhuis) Should be per-isolate or per-context, not global.
  uv_async_init(uv_default_loop(),
                &dispatch_debug_messages_async,
                DispatchDebugMessagesAsyncCallback);
  uv_unref(reinterpret_cast<uv_handle_t*>(&dispatch_debug_messages_async));

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

  const char typed_arrays_flag[] = "--harmony_typed_arrays";
  V8::SetFlagsFromString(typed_arrays_flag, sizeof(typed_arrays_flag) - 1);
  V8::SetArrayBufferAllocator(&ArrayBufferAllocator::the_singleton);

  // Fetch a reference to the main isolate, so we have a reference to it
  // even when we need it to access it from another (debugger) thread.
  node_isolate = Isolate::GetCurrent();

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
  RegisterSignalHandler(SIGINT, SignalExit);
  RegisterSignalHandler(SIGTERM, SignalExit);
#endif  // __POSIX__

  V8::SetFatalErrorHandler(node::OnFatalError);
  V8::AddMessageListener(OnMessage);

  // If the --debug flag was specified then initialize the debug thread.
  if (use_debug_agent) {
    EnableDebug(debug_wait_connect);
  } else {
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


void EmitExit(Environment* env) {
  // process.emit('exit')
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Object> process_object = env->process_object();
  process_object->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_exiting"),
                      True(node_isolate));

  Handle<String> exitCode = FIXED_ONE_BYTE_STRING(node_isolate, "exitCode");
  int code = process_object->Get(exitCode)->IntegerValue();

  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(node_isolate, "exit"),
    Integer::New(code, node_isolate)
  };

  MakeCallback(env, process_object, "emit", ARRAY_SIZE(args), args);
  exit(code);
}


Environment* CreateEnvironment(Isolate* isolate,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  HandleScope handle_scope(isolate);

  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);
  Environment* env = Environment::New(context);

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

  if (v8_is_profiling) {
    StartProfilerIdleNotifier(env);
  }

  Local<FunctionTemplate> process_template = FunctionTemplate::New();
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "process"));

  Local<Object> process_object = process_template->GetFunction()->NewInstance();
  env->set_process_object(process_object);

  SetupProcessObject(env, argc, argv, exec_argc, exec_argv);
  Load(env);

  return env;
}


int Start(int argc, char** argv) {
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

  V8::Initialize();
  {
    Locker locker(node_isolate);
    Environment* env =
        CreateEnvironment(node_isolate, argc, argv, exec_argc, exec_argv);
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());
    uv_run(env->event_loop(), UV_RUN_DEFAULT);
    EmitExit(env);
    RunAtExit(env);
    env->Dispose();
    env = NULL;
  }

#ifndef NDEBUG
  // Clean up. Not strictly necessary.
  V8::Dispose();
#endif  // NDEBUG

  delete[] exec_argv;
  exec_argv = NULL;

  return 0;
}


}  // namespace node

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
#include "req_wrap.h"
#include "handle_wrap.h"

#include "ares.h"
#include "uv.h"

#include "v8-debug.h"
#if defined HAVE_DTRACE || defined HAVE_ETW || defined HAVE_SYSTEMTAP
# include "node_dtrace.h"
#endif
#if defined HAVE_PERFCTR
# include "node_counters.h"
#endif

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_MSC_VER)
#include <strings.h>
#else
#define strcasecmp _stricmp
#endif
#include <limits.h> /* PATH_MAX */
#include <assert.h>
#if !defined(_MSC_VER)
#include <unistd.h> /* setuid, getuid */
#else
#include <direct.h>
#include <process.h>
#define getpid _getpid
#include <io.h>
#define umask _umask
typedef int mode_t;
#endif
#include <errno.h>
#include <sys/types.h>
#include "zlib.h"

#ifdef __POSIX__
# include <pwd.h> /* getpwnam() */
# include <grp.h> /* getgrnam() */
#endif

#include "node_buffer.h"
#include "node_file.h"
#include "node_http_parser.h"
#include "node_constants.h"
#include "node_javascript.h"
#include "node_version.h"
#include "node_string.h"
#if HAVE_OPENSSL
# include "node_crypto.h"
#endif
#if HAVE_SYSTEMTAP
#include "node_systemtap.h"
#endif
#include "node_script.h"
#include "v8_typed_array.h"

using namespace v8;

# ifdef __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
# elif !defined(_MSC_VER)
extern char **environ;
# endif

namespace node {

ngx_queue_t handle_wrap_queue = { &handle_wrap_queue, &handle_wrap_queue };
ngx_queue_t req_wrap_queue = { &req_wrap_queue, &req_wrap_queue };

// declared in req_wrap.h
Persistent<String> process_symbol;
Persistent<String> domain_symbol;

// declared in node_internals.h
Persistent<Object> process;

static Persistent<Function> process_tickFromSpinner;
static Persistent<Function> process_tickCallback;

static Persistent<String> exports_symbol;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> errpath_symbol;
static Persistent<String> code_symbol;

static Persistent<String> rss_symbol;
static Persistent<String> heap_total_symbol;
static Persistent<String> heap_used_symbol;

static Persistent<String> fatal_exception_symbol;

static Persistent<String> enter_symbol;
static Persistent<String> exit_symbol;
static Persistent<String> disposed_symbol;


static bool print_eval = false;
static bool force_repl = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static char *eval_string = NULL;
static int option_end_index = 0;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static int debug_port=5858;
static int max_stack_size = 0;
static bool using_domains = false;

// used by C++ modules as well
bool no_deprecation = false;

static uv_idle_t tick_spinner;
static bool need_tick_cb;
static Persistent<String> tick_callback_sym;

static uv_check_t check_immediate_watcher;
static uv_idle_t idle_immediate_dummy;
static bool need_immediate_cb;
static Persistent<String> immediate_callback_sym;

// for quick ref to tickCallback values
static struct {
  uint32_t length;
  uint32_t index;
  uint32_t depth;
} tick_infobox;

#ifdef OPENSSL_NPN_NEGOTIATED
static bool use_npn = true;
#else
static bool use_npn = false;
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
static bool use_sni = true;
#else
static bool use_sni = false;
#endif

// process-relative uptime base, initialized at start-up
static double prog_start_time;

static volatile bool debugger_running = false;
static uv_async_t dispatch_debug_messages_async;

// Declared in node_internals.h
Isolate* node_isolate = NULL;


static void Spin(uv_idle_t* handle, int status) {
  assert((uv_idle_t*) handle == &tick_spinner);
  assert(status == 0);

  // Avoid entering a V8 scope.
  if (!need_tick_cb) return;
  need_tick_cb = false;

  uv_idle_stop(&tick_spinner);

  HandleScope scope;

  if (process_tickFromSpinner.IsEmpty()) {
    Local<Value> cb_v = process->Get(String::New("_tickFromSpinner"));
    if (!cb_v->IsFunction()) {
      fprintf(stderr, "process._tickFromSpinner assigned to non-function\n");
      abort();
    }
    Local<Function> cb = cb_v.As<Function>();
    process_tickFromSpinner = Persistent<Function>::New(cb);
  }

  TryCatch try_catch;

  process_tickFromSpinner->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


static Handle<Value> NeedTickCallback(const Arguments& args) {
  need_tick_cb = true;
  uv_idle_start(&tick_spinner, Spin);
  return Undefined();
}


static void CheckImmediate(uv_check_t* handle, int status) {
  assert(handle == &check_immediate_watcher);
  assert(status == 0);

  HandleScope scope;

  if (immediate_callback_sym.IsEmpty()) {
    immediate_callback_sym = NODE_PSYMBOL("_immediateCallback");
  }

  MakeCallback(process, immediate_callback_sym, 0, NULL);
}


static void IdleImmediateDummy(uv_idle_t* handle, int status) {
  // Do nothing. Only for maintaining event loop
  assert(handle == &idle_immediate_dummy);
  assert(status == 0);
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
  Local<Value> e;
  Local<String> estring = String::NewSymbol(errno_string(errorno));
  if (!msg[0]) {
    msg = strerror(errorno);
  }
  Local<String> message = String::NewSymbol(msg);

  Local<String> cons1 = String::Concat(estring, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = NODE_PSYMBOL("syscall");
    errno_symbol = NODE_PSYMBOL("errno");
    errpath_symbol = NODE_PSYMBOL("path");
    code_symbol = NODE_PSYMBOL("code");
  }

  if (path) {
    Local<String> cons3 = String::Concat(cons2, String::NewSymbol(" '"));
    Local<String> cons4 = String::Concat(cons3, String::New(path));
    Local<String> cons5 = String::Concat(cons4, String::NewSymbol("'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();

  obj->Set(errno_symbol, Integer::New(errorno));
  obj->Set(code_symbol, estring);
  if (path) obj->Set(errpath_symbol, String::New(path));
  if (syscall) obj->Set(syscall_symbol, String::NewSymbol(syscall));
  return e;
}


static const char* get_uv_errno_string(int errorno) {
  uv_err_t err;
  memset(&err, 0, sizeof err);
  err.code = (uv_err_code)errorno;
  return uv_err_name(err);
}


static const char* get_uv_errno_message(int errorno) {
  uv_err_t err;
  memset(&err, 0, sizeof err);
  err.code = (uv_err_code)errorno;
  return uv_strerror(err);
}


// hack alert! copy of ErrnoException, tuned for uv errors
Local<Value> UVException(int errorno,
                         const char *syscall,
                         const char *msg,
                         const char *path) {
  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = NODE_PSYMBOL("syscall");
    errno_symbol = NODE_PSYMBOL("errno");
    errpath_symbol = NODE_PSYMBOL("path");
    code_symbol = NODE_PSYMBOL("code");
  }

  if (!msg || !msg[0])
    msg = get_uv_errno_message(errorno);

  Local<String> estring = String::NewSymbol(get_uv_errno_string(errorno));
  Local<String> message = String::NewSymbol(msg);
  Local<String> cons1 = String::Concat(estring, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e;

  Local<String> path_str;

  if (path) {
#ifdef _WIN32
    if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
      path_str = String::Concat(String::New("\\\\"), String::New(path + 8));
    } else if (strncmp(path, "\\\\?\\", 4) == 0) {
      path_str = String::New(path + 4);
    } else {
      path_str = String::New(path);
    }
#else
    path_str = String::New(path);
#endif

    Local<String> cons3 = String::Concat(cons2, String::NewSymbol(" '"));
    Local<String> cons4 = String::Concat(cons3, path_str);
    Local<String> cons5 = String::Concat(cons4, String::NewSymbol("'"));
    e = Exception::Error(cons5);
  } else {
    e = Exception::Error(cons2);
  }

  Local<Object> obj = e->ToObject();

  // TODO errno should probably go
  obj->Set(errno_symbol, Integer::New(errorno));
  obj->Set(code_symbol, estring);
  if (path) obj->Set(errpath_symbol, path_str);
  if (syscall) obj->Set(syscall_symbol, String::NewSymbol(syscall));
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
  Local<Value> e;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno);
  }
  Local<String> message = String::NewSymbol(msg);

  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = NODE_PSYMBOL("syscall");
    errno_symbol = NODE_PSYMBOL("errno");
    errpath_symbol = NODE_PSYMBOL("path");
    code_symbol = NODE_PSYMBOL("code");
  }

  if (path) {
    Local<String> cons1 = String::Concat(message, String::NewSymbol(" '"));
    Local<String> cons2 = String::Concat(cons1, String::New(path));
    Local<String> cons3 = String::Concat(cons2, String::NewSymbol("'"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Object> obj = e->ToObject();

  obj->Set(errno_symbol, Integer::New(errorno));
  if (path) obj->Set(errpath_symbol, String::New(path));
  if (syscall) obj->Set(syscall_symbol, String::NewSymbol(syscall));
  return e;
}
#endif


Handle<Value> FromConstructorTemplate(Persistent<FunctionTemplate> t,
                                      const Arguments& args) {
  HandleScope scope;
  Local<Value> argv[32];
  unsigned argc = args.Length();
  if (argc > ARRAY_SIZE(argv)) argc = ARRAY_SIZE(argv);
  for (unsigned i = 0; i < argc; ++i) argv[i] = args[i];
  return scope.Close(t->GetFunction()->NewInstance(argc, argv));
}


Handle<Value> UsingDomains(const Arguments& args) {
  HandleScope scope;
  if (using_domains)
    return scope.Close(Undefined());
  using_domains = true;
  Local<Value> tdc_v = process->Get(String::New("_tickDomainCallback"));
  Local<Value> ndt_v = process->Get(String::New("_nextDomainTick"));
  if (!tdc_v->IsFunction()) {
    fprintf(stderr, "process._tickDomainCallback assigned to non-function\n");
    abort();
  }
  if (!ndt_v->IsFunction()) {
    fprintf(stderr, "process._nextDomainTick assigned to non-function\n");
    abort();
  }
  Local<Function> tdc = tdc_v.As<Function>();
  Local<Function> ndt = ndt_v.As<Function>();
  process->Set(String::New("_tickCallback"), tdc);
  process->Set(String::New("nextTick"), ndt);
  process_tickCallback = Persistent<Function>::New(tdc);
  return Undefined();
}


Handle<Value>
MakeDomainCallback(const Handle<Object> object,
                   const Handle<Function> callback,
                   int argc,
                   Handle<Value> argv[]) {
  // TODO Hook for long stack traces to be made here.

  // lazy load domain specific symbols
  if (enter_symbol.IsEmpty()) {
    enter_symbol = NODE_PSYMBOL("enter");
    exit_symbol = NODE_PSYMBOL("exit");
    disposed_symbol = NODE_PSYMBOL("_disposed");
  }

  Local<Value> domain_v = object->Get(domain_symbol);
  Local<Object> domain;
  Local<Function> enter;
  Local<Function> exit;

  TryCatch try_catch;

  bool has_domain = domain_v->IsObject();
  if (has_domain) {
    domain = domain_v->ToObject();
    assert(!domain.IsEmpty());
    if (domain->Get(disposed_symbol)->IsTrue()) {
      // domain has been disposed of.
      return Undefined();
    }
    enter = Local<Function>::Cast(domain->Get(enter_symbol));
    assert(!enter.IsEmpty());
    enter->Call(domain, 0, NULL);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return Undefined();
    }
  }

  Local<Value> ret = callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
    return Undefined();
  }

  if (has_domain) {
    exit = Local<Function>::Cast(domain->Get(exit_symbol));
    assert(!exit.IsEmpty());
    exit->Call(domain, 0, NULL);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return Undefined();
    }
  }

  if (tick_infobox.length == 0) {
    tick_infobox.index = 0;
    tick_infobox.depth = 0;
    return ret;
  }

  // process nextTicks after call
  process_tickCallback->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
    return Undefined();
  }

  return ret;
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const Handle<Function> callback,
             int argc,
             Handle<Value> argv[]) {
  // TODO Hook for long stack traces to be made here.

  // lazy load no domain next tick callbacks
  if (process_tickCallback.IsEmpty()) {
    Local<Value> cb_v = process->Get(String::New("_tickCallback"));
    if (!cb_v->IsFunction()) {
      fprintf(stderr, "process._tickCallback assigned to non-function\n");
      abort();
    }
    Local<Function> cb = cb_v.As<Function>();
    process_tickCallback = Persistent<Function>::New(cb);
  }

  TryCatch try_catch;

  Local<Value> ret = callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
    return Undefined();
  }

  if (tick_infobox.length == 0) {
    tick_infobox.index = 0;
    tick_infobox.depth = 0;
    return ret;
  }

  // process nextTicks after call
  process_tickCallback->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
    return Undefined();
  }

  return ret;
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const Handle<String> symbol,
             int argc,
             Handle<Value> argv[]) {
  HandleScope scope;

  Local<Function> callback = object->Get(symbol).As<Function>();

  if (using_domains)
    return scope.Close(MakeDomainCallback(object, callback, argc, argv));
  return scope.Close(MakeCallback(object, callback, argc, argv));
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const char* method,
             int argc,
             Handle<Value> argv[]) {
  HandleScope scope;

  Handle<Value> ret =
    MakeCallback(object, String::NewSymbol(method), argc, argv);

  return scope.Close(ret);
}


void SetErrno(uv_err_t err) {
  HandleScope scope;

  static Persistent<String> errno_symbol;
  if (errno_symbol.IsEmpty()) {
    errno_symbol = NODE_PSYMBOL("_errno");
  }

  if (err.code == UV_UNKNOWN) {
    char errno_buf[100];
    snprintf(errno_buf, 100, "Unknown system errno %d", err.sys_errno_);
    process->Set(errno_symbol, String::New(errno_buf));
  } else {
    process->Set(errno_symbol, String::NewSymbol(uv_err_name(err)));
  }
}


enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope;

  if (!encoding_v->IsString()) return _default;

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
  HandleScope scope;

  if (encoding == BUFFER) {
    return scope.Close(
        Buffer::New(static_cast<const char*>(buf), len)->handle_);
  }

  if (!len) return scope.Close(String::Empty());

  if (encoding == BINARY) {
    const unsigned char *cbuf = static_cast<const unsigned char*>(buf);
    uint16_t * twobytebuf = new uint16_t[len];
    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }
    Local<String> chunk = String::New(twobytebuf, len);
    delete [] twobytebuf; // TODO use ExternalTwoByteString?
    return scope.Close(chunk);
  }

  // utf8 or ascii encoding
  Local<String> chunk = String::New((const char*)buf, len);
  return scope.Close(chunk);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(v8::Handle<v8::Value> val, enum encoding encoding) {
  HandleScope scope;

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  if ((encoding == BUFFER || encoding == BINARY) && Buffer::HasInstance(val)) {
    return Buffer::Length(val->ToObject());
  }

  Local<String> str = val->ToString();

  if (encoding == UTF8) return str->Utf8Length();
  else if (encoding == UCS2) return str->Length() * 2;
  else if (encoding == HEX) return str->Length() / 2;

  return str->Length();
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t DecodeWrite(char *buf,
                    size_t buflen,
                    v8::Handle<v8::Value> val,
                    enum encoding encoding) {
  HandleScope scope;

  // XXX
  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  bool is_buffer = Buffer::HasInstance(val);

  if (is_buffer && (encoding == BINARY || encoding == BUFFER)) {
    // fast path, copy buffer data
    const char* data = Buffer::Data(val.As<Object>());
    size_t size = Buffer::Length(val.As<Object>());
    size_t len = size < buflen ? size : buflen;
    memcpy(buf, data, len);
    return len;
  }

  Local<String> str;

  if (is_buffer) { // slow path, convert to binary string
    Local<Value> arg = String::New("binary");
    str = MakeCallback(val.As<Object>(), "toString", 1, &arg)->ToString();
  }
  else {
    str = val->ToString();
  }

  if (encoding == UTF8) {
    str->WriteUtf8(buf, buflen, NULL, String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  if (encoding == ASCII) {
    str->WriteAscii(buf, 0, buflen, String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME

  assert(encoding == BINARY);

  uint16_t * twobytebuf = new uint16_t[buflen];

  str->Write(twobytebuf, 0, buflen, String::HINT_MANY_WRITES_EXPECTED);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    buf[i] = b[0];
  }

  delete [] twobytebuf;

  return buflen;
}

void DisplayExceptionLine (TryCatch &try_catch) {
  // Prevent re-entry into this function.  For example, if there is
  // a throw from a program in vm.runInThisContext(code, filename, true),
  // then we want to show the original failure, not the secondary one.
  static bool displayed_error = false;

  if (displayed_error) return;
  displayed_error = true;

  HandleScope scope;

  Handle<Message> message = try_catch.Message();

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

    // fprintf(stderr, "---\nsourceline:%s\noffset:%d\nstart:%d\nend:%d\n---\n", sourceline_string, start, end);

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


static void ReportException(TryCatch &try_catch, bool show_line) {
  HandleScope scope;

  if (show_line) DisplayExceptionLine(try_catch);

  String::Utf8Value trace(try_catch.StackTrace());

  // range errors have a trace member set to undefined
  if (trace.length() > 0 && !try_catch.StackTrace()->IsUndefined()) {
    fprintf(stderr, "%s\n", *trace);
  } else {
    // this really only happens for RangeErrors, since they're the only
    // kind that won't have all this info in the trace, or when non-Error
    // objects are thrown manually.
    Local<Value> er = try_catch.Exception();
    bool isErrorObject = er->IsObject() &&
      !(er->ToObject()->Get(String::New("message"))->IsUndefined()) &&
      !(er->ToObject()->Get(String::New("name"))->IsUndefined());

    if (isErrorObject) {
      String::Utf8Value name(er->ToObject()->Get(String::New("name")));
      fprintf(stderr, "%s: ", *name);
    }

    String::Utf8Value msg(!isErrorObject ? er
                         : er->ToObject()->Get(String::New("message")));
    fprintf(stderr, "%s\n", *msg);
  }

  fflush(stderr);
}

// Executes a str within the current v8 context.
Local<Value> ExecuteString(Handle<String> source, Handle<Value> filename) {
  HandleScope scope;
  TryCatch try_catch;

  Local<v8::Script> script = v8::Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(try_catch, true);
    exit(3);
  }

  Local<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(try_catch, true);
    exit(4);
  }

  return scope.Close(result);
}


static Handle<Value> GetActiveRequests(const Arguments& args) {
  HandleScope scope;

  Local<Array> ary = Array::New();
  ngx_queue_t* q = NULL;
  int i = 0;

  ngx_queue_foreach(q, &req_wrap_queue) {
    ReqWrap<uv_req_t>* w = container_of(q, ReqWrap<uv_req_t>, req_wrap_queue_);
    if (w->object_.IsEmpty()) continue;
    ary->Set(i++, w->object_);
  }

  return scope.Close(ary);
}


// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
Handle<Value> GetActiveHandles(const Arguments& args) {
  HandleScope scope;

  Local<Array> ary = Array::New();
  ngx_queue_t* q = NULL;
  int i = 0;

  Local<String> owner_sym = String::New("owner");

  ngx_queue_foreach(q, &handle_wrap_queue) {
    HandleWrap* w = container_of(q, HandleWrap, handle_wrap_queue_);
    if (w->object_.IsEmpty() || (w->flags_ & HandleWrap::kUnref)) continue;
    Local<Value> obj = w->object_->Get(owner_sym);
    if (obj->IsUndefined()) obj = *w->object_;
    ary->Set(i++, obj);
  }

  return scope.Close(ary);
}


static Handle<Value> Abort(const Arguments& args) {
  abort();
  return Undefined();
}


static Handle<Value> Chdir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }

  String::Utf8Value path(args[0]);

  uv_err_t r = uv_chdir(*path);

  if (r.code != UV_OK) {
    return ThrowException(UVException(r.code, "uv_chdir"));
  }

  return Undefined();
}


static Handle<Value> Cwd(const Arguments& args) {
  HandleScope scope;
#ifdef _WIN32
  /* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
  char buf[MAX_PATH * 4 + 1];
#else
  char buf[PATH_MAX + 1];
#endif

  uv_err_t r = uv_cwd(buf, ARRAY_SIZE(buf) - 1);
  if (r.code != UV_OK) {
    return ThrowException(UVException(r.code, "uv_cwd"));
  }

  buf[ARRAY_SIZE(buf) - 1] = '\0';
  Local<String> cwd = String::New(buf);

  return scope.Close(cwd);
}


static Handle<Value> Umask(const Arguments& args) {
  HandleScope scope;
  unsigned int old;

  if (args.Length() < 1 || args[0]->IsUndefined()) {
    old = umask(0);
    umask((mode_t)old);

  } else if(!args[0]->IsInt32() && !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("argument must be an integer or octal string.")));

  } else {
    int oct;
    if(args[0]->IsInt32()) {
      oct = args[0]->Uint32Value();
    } else {
      oct = 0;
      String::Utf8Value str(args[0]);

      // Parse the octal string.
      for (int i = 0; i < str.length(); i++) {
        char c = (*str)[i];
        if (c > '7' || c < '0') {
          return ThrowException(Exception::TypeError(
                String::New("invalid octal string")));
        }
        oct *= 8;
        oct += c - '0';
      }
    }
    old = umask(static_cast<mode_t>(oct));
  }

  return scope.Close(Uint32::New(old));
}


#ifdef __POSIX__

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


static Handle<Value> GetUid(const Arguments& args) {
  HandleScope scope;
  int uid = getuid();
  return scope.Close(Integer::New(uid));
}


static Handle<Value> GetGid(const Arguments& args) {
  HandleScope scope;
  int gid = getgid();
  return scope.Close(Integer::New(gid));
}


static Handle<Value> SetGid(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return ThrowTypeError("setgid argument must be a number or a string");
  }

  gid_t gid = gid_by_name(args[0]);

  if (gid == gid_not_found) {
    return ThrowError("setgid group id does not exist");
  }

  if (setgid(gid)) {
    return ThrowException(ErrnoException(errno, "setgid"));
  }

  return Undefined();
}


static Handle<Value> SetUid(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return ThrowTypeError("setuid argument must be a number or a string");
  }

  uid_t uid = uid_by_name(args[0]);

  if (uid == uid_not_found) {
    return ThrowError("setuid user id does not exist");
  }

  if (setuid(uid)) {
    return ThrowException(ErrnoException(errno, "setuid"));
  }

  return Undefined();
}


static Handle<Value> GetGroups(const Arguments& args) {
  HandleScope scope;

  int ngroups = getgroups(0, NULL);

  if (ngroups == -1) {
    return ThrowException(ErrnoException(errno, "getgroups"));
  }

  gid_t* groups = new gid_t[ngroups];

  ngroups = getgroups(ngroups, groups);

  if (ngroups == -1) {
    delete[] groups;
    return ThrowException(ErrnoException(errno, "getgroups"));
  }

  Local<Array> groups_list = Array::New(ngroups);
  bool seen_egid = false;
  gid_t egid = getegid();

  for (int i = 0; i < ngroups; i++) {
    groups_list->Set(i, Integer::New(groups[i]));
    if (groups[i] == egid) seen_egid = true;
  }

  delete[] groups;

  if (seen_egid == false) {
    groups_list->Set(ngroups, Integer::New(egid));
  }

  return scope.Close(groups_list);
}


static Handle<Value> SetGroups(const Arguments& args) {
  HandleScope scope;

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
    return ThrowException(ErrnoException(errno, "setgroups"));
  }

  return Undefined();
}


static Handle<Value> InitGroups(const Arguments& args) {
  HandleScope scope;

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
    if (must_free) free(user);
    return ThrowError("initgroups extra group not found");
  }

  int rc = initgroups(user, extra_group);

  if (must_free) {
    free(user);
  }

  if (rc) {
    return ThrowException(ErrnoException(errno, "initgroups"));
  }

  return Undefined();
}

#endif // __POSIX__


v8::Handle<v8::Value> Exit(const v8::Arguments& args) {
  HandleScope scope;
  exit(args[0]->IntegerValue());
  return Undefined();
}


static Handle<Value> Uptime(const Arguments& args) {
  HandleScope scope;
  double uptime;

  uv_err_t err = uv_uptime(&uptime);

  if (err.code != UV_OK) {
    return Undefined();
  }

  return scope.Close(Number::New(uptime - prog_start_time));
}


v8::Handle<v8::Value> MemoryUsage(const v8::Arguments& args) {
  HandleScope scope;

  size_t rss;

  uv_err_t err = uv_resident_set_memory(&rss);

  if (err.code != UV_OK) {
    return ThrowException(UVException(err.code, "uv_resident_set_memory"));
  }

  Local<Object> info = Object::New();

  if (rss_symbol.IsEmpty()) {
    rss_symbol = NODE_PSYMBOL("rss");
    heap_total_symbol = NODE_PSYMBOL("heapTotal");
    heap_used_symbol = NODE_PSYMBOL("heapUsed");
  }

  info->Set(rss_symbol, Number::New(rss));

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  V8::GetHeapStatistics(&v8_heap_stats);
  info->Set(heap_total_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.total_heap_size()));
  info->Set(heap_used_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.used_heap_size()));

  return scope.Close(info);
}


Handle<Value> Kill(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 2) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }

  int pid = args[0]->IntegerValue();
  int sig = args[1]->Int32Value();
  uv_err_t err = uv_kill(pid, sig);

  if (err.code != UV_OK) {
    SetErrno(err);
    return scope.Close(Integer::New(-1));
  }

  return Undefined();
}

// used in Hrtime() below
#define NANOS_PER_SEC 1000000000

// Hrtime exposes libuv's uv_hrtime() high-resolution timer.
// The value returned by uv_hrtime() is a 64-bit int representing nanoseconds,
// so this function instead returns an Array with 2 entries representing seconds
// and nanoseconds, to avoid any integer overflow possibility.
// Pass in an Array from a previous hrtime() call to instead get a time diff.
Handle<Value> Hrtime(const v8::Arguments& args) {
  HandleScope scope;

  uint64_t t = uv_hrtime();

  if (args.Length() > 0) {
    // return a time diff tuple
    if (!args[0]->IsArray()) {
      Local<Value> exception = Exception::TypeError(
          String::New("process.hrtime() only accepts an Array tuple."));
      return ThrowException(exception);
    }
    Local<Array> inArray = Local<Array>::Cast(args[0]);
    uint64_t seconds = inArray->Get(0)->Uint32Value();
    uint64_t nanos = inArray->Get(1)->Uint32Value();
    t -= (seconds * NANOS_PER_SEC) + nanos;
  }

  Local<Array> tuple = Array::New(2);
  tuple->Set(0, Integer::NewFromUnsigned(t / NANOS_PER_SEC));
  tuple->Set(1, Integer::NewFromUnsigned(t % NANOS_PER_SEC));

  return scope.Close(tuple);
}


typedef void (UV_DYNAMIC* extInit)(Handle<Object> exports);

// DLOpen is process.dlopen(module, filename).
// Used to load 'module.node' dynamically shared objects.
Handle<Value> DLOpen(const v8::Arguments& args) {
  HandleScope scope;
  char symbol[1024], *base, *pos;
  uv_lib_t lib;
  int r;

  if (args.Length() < 2) {
    Local<Value> exception = Exception::Error(
        String::New("process.dlopen takes exactly 2 arguments."));
    return ThrowException(exception);
  }

  Local<Object> module = args[0]->ToObject(); // Cast
  String::Utf8Value filename(args[1]); // Cast

  if (exports_symbol.IsEmpty()) {
    exports_symbol = NODE_PSYMBOL("exports");
  }
  Local<Object> exports = module->Get(exports_symbol)->ToObject();

  if (uv_dlopen(*filename, &lib)) {
    Local<String> errmsg = String::New(uv_dlerror(&lib));
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, args[1]->ToString());
#endif
    return ThrowException(Exception::Error(errmsg));
  }

  String::Utf8Value path(args[1]);
  base = *path;

  /* Find the shared library filename within the full path. */
#ifdef __POSIX__
  pos = strrchr(base, '/');
  if (pos != NULL) {
    base = pos + 1;
  }
#else // Windows
  for (;;) {
    pos = strpbrk(base, "\\/:");
    if (pos == NULL) {
      break;
    }
    base = pos + 1;
  }
#endif

  /* Strip the .node extension. */
  pos = strrchr(base, '.');
  if (pos != NULL) {
    *pos = '\0';
  }

  /* Add the `_module` suffix to the extension name. */
  r = snprintf(symbol, sizeof symbol, "%s_module", base);
  if (r <= 0 || static_cast<size_t>(r) >= sizeof symbol) {
    Local<Value> exception =
        Exception::Error(String::New("Out of memory."));
    return ThrowException(exception);
  }

  /* Replace dashes with underscores. When loading foo-bar.node,
   * look for foo_bar_module, not foo-bar_module.
   */
  for (pos = symbol; *pos != '\0'; ++pos) {
    if (*pos == '-') *pos = '_';
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
  mod->register_func(exports, module);

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
  return Undefined();
}


static void OnFatalError(const char* location, const char* message) {
  if (location) {
    fprintf(stderr, "FATAL ERROR: %s %s\n", location, message);
  } else {
    fprintf(stderr, "FATAL ERROR: %s\n", message);
  }
  exit(5);
}

void FatalException(TryCatch &try_catch) {
  HandleScope scope;

  if (fatal_exception_symbol.IsEmpty())
    fatal_exception_symbol = NODE_PSYMBOL("_fatalException");

  Local<Value> fatal_v = process->Get(fatal_exception_symbol);

  if (!fatal_v->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(try_catch, true);
    exit(6);
  }

  Local<Function> fatal_f = Local<Function>::Cast(fatal_v);

  Local<Value> error = try_catch.Exception();
  Local<Value> argv[] = { error };

  TryCatch fatal_try_catch;

  // this will return true if the JS layer handled it, false otherwise
  Local<Value> caught = fatal_f->Call(process, ARRAY_SIZE(argv), argv);

  if (fatal_try_catch.HasCaught()) {
    // the fatal exception function threw, so we must exit
    ReportException(fatal_try_catch, true);
    exit(7);
  }

  if (false == caught->BooleanValue()) {
    ReportException(try_catch, true);
    exit(8);
  }
}


Persistent<Object> binding_cache;
Persistent<Array> module_load_list;

static Handle<Value> Binding(const Arguments& args) {
  HandleScope scope;

  Local<String> module = args[0]->ToString();
  String::Utf8Value module_v(module);
  node_module_struct* modp;

  if (binding_cache.IsEmpty()) {
    binding_cache = Persistent<Object>::New(Object::New());
  }

  Local<Object> exports;

  if (binding_cache->Has(module)) {
    exports = binding_cache->Get(module)->ToObject();
    return scope.Close(exports);
  }

  // Append a string to process.moduleLoadList
  char buf[1024];
  snprintf(buf, 1024, "Binding %s", *module_v);
  uint32_t l = module_load_list->Length();
  module_load_list->Set(l, String::New(buf));

  if ((modp = get_builtin_module(*module_v)) != NULL) {
    exports = Object::New();
    // Internal bindings don't have a "module" object,
    // only exports.
    modp->register_func(exports, Undefined());
    binding_cache->Set(module, exports);

  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New();
    DefineConstants(exports);
    binding_cache->Set(module, exports);

  } else if (!strcmp(*module_v, "natives")) {
    exports = Object::New();
    DefineJavaScript(exports);
    binding_cache->Set(module, exports);

  } else {

    return ThrowException(Exception::Error(String::New("No such module")));
  }

  return scope.Close(exports);
}


static Handle<Value> ProcessTitleGetter(Local<String> property,
                                        const AccessorInfo& info) {
  HandleScope scope;
  char buffer[512];
  uv_get_process_title(buffer, sizeof(buffer));
  return scope.Close(String::New(buffer));
}


static void ProcessTitleSetter(Local<String> property,
                               Local<Value> value,
                               const AccessorInfo& info) {
  HandleScope scope;
  String::Utf8Value title(value);
  // TODO: protect with a lock
  uv_set_process_title(*title);
}


static Handle<Value> EnvGetter(Local<String> property,
                               const AccessorInfo& info) {
  HandleScope scope;
#ifdef __POSIX__
  String::Utf8Value key(property);
  const char* val = getenv(*key);
  if (val) {
    return scope.Close(String::New(val));
  }
#else  // _WIN32
  String::Value key(property);
  WCHAR buffer[32767]; // The maximum size allowed for environment variables.
  DWORD result = GetEnvironmentVariableW(reinterpret_cast<WCHAR*>(*key),
                                         buffer,
                                         ARRAY_SIZE(buffer));
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // not found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) &&
      result < ARRAY_SIZE(buffer)) {
    return scope.Close(String::New(reinterpret_cast<uint16_t*>(buffer), result));
  }
#endif
  // Not found.  Fetch from prototype.
  return scope.Close(info.Data().As<Object>()->Get(property));
}


static Handle<Value> EnvSetter(Local<String> property,
                               Local<Value> value,
                               const AccessorInfo& info) {
  HandleScope scope;
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
  return scope.Close(value);
}


static Handle<Integer> EnvQuery(Local<String> property,
                                const AccessorInfo& info) {
  HandleScope scope;
#ifdef __POSIX__
  String::Utf8Value key(property);
  if (getenv(*key)) {
    return scope.Close(Integer::New(0));
  }
#else  // _WIN32
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (GetEnvironmentVariableW(key_ptr, NULL, 0) > 0 ||
      GetLastError() == ERROR_SUCCESS) {
    if (key_ptr[0] == L'=') {
      // Environment variables that start with '=' are hidden and read-only.
      return scope.Close(Integer::New(v8::ReadOnly ||
                                      v8::DontDelete ||
                                      v8::DontEnum));
    } else {
      return scope.Close(Integer::New(0));
    }
  }
#endif
  // Not found
  return scope.Close(Handle<Integer>());
}


static Handle<Boolean> EnvDeleter(Local<String> property,
                                  const AccessorInfo& info) {
  HandleScope scope;
#ifdef __POSIX__
  String::Utf8Value key(property);
  if (!getenv(*key)) return False();
  unsetenv(*key); // can't check return value, it's void on some platforms
  return True();
#else
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  if (key_ptr[0] == L'=' || !SetEnvironmentVariableW(key_ptr, NULL)) {
    // Deletion failed. Return true if the key wasn't there in the first place,
    // false if it is still there.
    bool rv = GetEnvironmentVariableW(key_ptr, NULL, NULL) == 0 &&
              GetLastError() != ERROR_SUCCESS;
    return scope.Close(Boolean::New(rv));
  }
  return True();
#endif
}


static Handle<Array> EnvEnumerator(const AccessorInfo& info) {
  HandleScope scope;
#ifdef __POSIX__
  int size = 0;
  while (environ[size]) size++;

  Local<Array> env = Array::New(size);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    env->Set(i, String::New(var, length));
  }
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == NULL) {
    // This should not happen.
    return scope.Close(Handle<Array>());
  }
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
    env->Set(i++, String::New(reinterpret_cast<uint16_t*>(p), s - p));
    p = s + wcslen(s) + 1;
  }
  FreeEnvironmentStringsW(environment);
#endif
  return scope.Close(env);
}


static Handle<Object> GetFeatures() {
  HandleScope scope;

  Local<Object> obj = Object::New();
  obj->Set(String::NewSymbol("debug"),
#if defined(DEBUG) && DEBUG
    True()
#else
    False()
#endif
  );

  obj->Set(String::NewSymbol("uv"), True());
  obj->Set(String::NewSymbol("ipv6"), True()); // TODO ping libuv
  obj->Set(String::NewSymbol("tls_npn"), Boolean::New(use_npn));
  obj->Set(String::NewSymbol("tls_sni"), Boolean::New(use_sni));
  obj->Set(String::NewSymbol("tls"),
      Boolean::New(get_builtin_module("crypto") != NULL));

  return scope.Close(obj);
}


static Handle<Value> DebugPortGetter(Local<String> property,
                                     const AccessorInfo& info) {
  HandleScope scope;
  return scope.Close(Integer::NewFromUnsigned(debug_port));
}


static void DebugPortSetter(Local<String> property,
                            Local<Value> value,
                            const AccessorInfo& info) {
  HandleScope scope;
  debug_port = value->NumberValue();
}


static Handle<Value> DebugProcess(const Arguments& args);
static Handle<Value> DebugPause(const Arguments& args);
static Handle<Value> DebugEnd(const Arguments& args);


Handle<Value> NeedImmediateCallbackGetter(Local<String> property,
                                          const AccessorInfo& info) {
  return Boolean::New(need_immediate_cb);
}


static void NeedImmediateCallbackSetter(Local<String> property,
                                        Local<Value> value,
                                        const AccessorInfo& info) {
  HandleScope scope;

  bool bool_value = value->BooleanValue();

  if (need_immediate_cb == bool_value) return;

  need_immediate_cb = bool_value;

  if (need_immediate_cb) {
    uv_check_start(&check_immediate_watcher, node::CheckImmediate);
    // idle handle is needed only to maintain event loop
    uv_idle_start(&idle_immediate_dummy, node::IdleImmediateDummy);
  } else {
    uv_check_stop(&check_immediate_watcher);
    uv_idle_stop(&idle_immediate_dummy);
  }
}


Handle<Object> SetupProcessObject(int argc, char *argv[]) {
  HandleScope scope;

  int i, j;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();

  process_template->SetClassName(String::NewSymbol("process"));

  process = Persistent<Object>::New(process_template->GetFunction()->NewInstance());

  process->SetAccessor(String::New("title"),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

  // process.version
  process->Set(String::NewSymbol("version"), String::New(NODE_VERSION));

  // process.moduleLoadList
  module_load_list = Persistent<Array>::New(Array::New());
  process->Set(String::NewSymbol("moduleLoadList"), module_load_list);

  // process.versions
  Local<Object> versions = Object::New();
  process->Set(String::NewSymbol("versions"), versions);
  versions->Set(String::NewSymbol("http_parser"), String::New(
               NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR) "."
               NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)));
  // +1 to get rid of the leading 'v'
  versions->Set(String::NewSymbol("node"), String::New(NODE_VERSION+1));
  versions->Set(String::NewSymbol("v8"), String::New(V8::GetVersion()));
  versions->Set(String::NewSymbol("ares"), String::New(ARES_VERSION_STR));
  versions->Set(String::NewSymbol("uv"), String::New(uv_version_string()));
  versions->Set(String::NewSymbol("zlib"), String::New(ZLIB_VERSION));
  versions->Set(String::NewSymbol("modules"),
                String::New(NODE_STRINGIFY(NODE_MODULE_VERSION)));
#if HAVE_OPENSSL
  // Stupid code to slice out the version string.
  int c, l = strlen(OPENSSL_VERSION_TEXT);
  for (i = j = 0; i < l; i++) {
    c = OPENSSL_VERSION_TEXT[i];
    if ('0' <= c && c <= '9') {
      for (j = i + 1; j < l; j++) {
        c = OPENSSL_VERSION_TEXT[j];
        if (c == ' ') break;
      }
      break;
    }
  }
  versions->Set(String::NewSymbol("openssl"),
                String::New(OPENSSL_VERSION_TEXT + i, j - i));
#endif



  // process.arch
  process->Set(String::NewSymbol("arch"), String::New(ARCH));

  // process.platform
  process->Set(String::NewSymbol("platform"), String::New(PLATFORM));

  // process.argv
  Local<Array> arguments = Array::New(argc - option_end_index + 1);
  arguments->Set(Integer::New(0), String::New(argv[0]));
  for (j = 1, i = option_end_index; i < argc; j++, i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(j), arg);
  }
  // assign it
  process->Set(String::NewSymbol("argv"), arguments);

  // process.execArgv
  Local<Array> execArgv = Array::New(option_end_index - 1);
  for (j = 1, i = 0; j < option_end_index; j++, i++) {
    execArgv->Set(Integer::New(i), String::New(argv[j]));
  }
  // assign it
  process->Set(String::NewSymbol("execArgv"), execArgv);


  // create process.env
  Local<ObjectTemplate> envTemplate = ObjectTemplate::New();
  envTemplate->SetNamedPropertyHandler(EnvGetter,
                                       EnvSetter,
                                       EnvQuery,
                                       EnvDeleter,
                                       EnvEnumerator,
                                       Object::New());
  Local<Object> env = envTemplate->NewInstance();
  process->Set(String::NewSymbol("env"), env);

  process->Set(String::NewSymbol("pid"), Integer::New(getpid()));
  process->Set(String::NewSymbol("features"), GetFeatures());
  process->SetAccessor(String::New("_needImmediateCallback"),
                       NeedImmediateCallbackGetter,
                       NeedImmediateCallbackSetter);

  // -e, --eval
  if (eval_string) {
    process->Set(String::NewSymbol("_eval"), String::New(eval_string));
  }

  // -p, --print
  if (print_eval) {
    process->Set(String::NewSymbol("_print_eval"), True());
  }

  // -i, --interactive
  if (force_repl) {
    process->Set(String::NewSymbol("_forceRepl"), True());
  }

  // --no-deprecation
  if (no_deprecation) {
    process->Set(String::NewSymbol("noDeprecation"), True());
  }

  // --throw-deprecation
  if (throw_deprecation) {
    process->Set(String::NewSymbol("throwDeprecation"), True());
  }

  // --trace-deprecation
  if (trace_deprecation) {
    process->Set(String::NewSymbol("traceDeprecation"), True());
  }

  size_t size = 2*PATH_MAX;
  char* execPath = new char[size];
  if (uv_exepath(execPath, &size) != 0) {
    // as a last ditch effort, fallback on argv[0] ?
    process->Set(String::NewSymbol("execPath"), String::New(argv[0]));
  } else {
    process->Set(String::NewSymbol("execPath"), String::New(execPath, size));
  }
  delete [] execPath;

  process->SetAccessor(String::New("debugPort"),
                       DebugPortGetter,
                       DebugPortSetter);


  // define various internal methods
  NODE_SET_METHOD(process, "_getActiveRequests", GetActiveRequests);
  NODE_SET_METHOD(process, "_getActiveHandles", GetActiveHandles);
  NODE_SET_METHOD(process, "_needTickCallback", NeedTickCallback);
  NODE_SET_METHOD(process, "reallyExit", Exit);
  NODE_SET_METHOD(process, "abort", Abort);
  NODE_SET_METHOD(process, "chdir", Chdir);
  NODE_SET_METHOD(process, "cwd", Cwd);

  NODE_SET_METHOD(process, "umask", Umask);

#ifdef __POSIX__
  NODE_SET_METHOD(process, "getuid", GetUid);
  NODE_SET_METHOD(process, "setuid", SetUid);

  NODE_SET_METHOD(process, "setgid", SetGid);
  NODE_SET_METHOD(process, "getgid", GetGid);

  NODE_SET_METHOD(process, "getgroups", GetGroups);
  NODE_SET_METHOD(process, "setgroups", SetGroups);
  NODE_SET_METHOD(process, "initgroups", InitGroups);
#endif // __POSIX__

  NODE_SET_METHOD(process, "_kill", Kill);

  NODE_SET_METHOD(process, "_debugProcess", DebugProcess);
  NODE_SET_METHOD(process, "_debugPause", DebugPause);
  NODE_SET_METHOD(process, "_debugEnd", DebugEnd);

  NODE_SET_METHOD(process, "hrtime", Hrtime);

  NODE_SET_METHOD(process, "dlopen", DLOpen);

  NODE_SET_METHOD(process, "uptime", Uptime);
  NODE_SET_METHOD(process, "memoryUsage", MemoryUsage);

  NODE_SET_METHOD(process, "binding", Binding);

  NODE_SET_METHOD(process, "_usingDomains", UsingDomains);

  // values use to cross communicate with processNextTick
  Local<Object> info_box = Object::New();
  info_box->SetIndexedPropertiesToExternalArrayData(&tick_infobox,
                                                    kExternalUnsignedIntArray,
                                                    3);
  process->Set(String::NewSymbol("_tickInfoBox"), info_box);

  // pre-set _events object for faster emit checks
  process->Set(String::NewSymbol("_events"), Object::New());

  return process;
}


static void AtExit() {
  uv_tty_reset_mode();
}


static void SignalExit(int signal) {
  uv_tty_reset_mode();
  _exit(128 + signal);
}


void Load(Handle<Object> process_l) {
  process_symbol = NODE_PSYMBOL("process");
  domain_symbol = NODE_PSYMBOL("domain");

  // Compile, execute the src/node.js file. (Which was included as static C
  // string in node_natives.h. 'natve_node' is the string containing that
  // source code.)

  // The node.js file returns a function 'f'
  atexit(AtExit);

  TryCatch try_catch;

  Local<Value> f_value = ExecuteString(MainSource(),
                                       IMMUTABLE_STRING("node.js"));
  if (try_catch.HasCaught())  {
    ReportException(try_catch, true);
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
  Local<Object> global = v8::Context::GetCurrent()->Global();
  Local<Value> args[1] = { Local<Value>::New(process_l) };

#if defined HAVE_DTRACE || defined HAVE_ETW || defined HAVE_SYSTEMTAP
  InitDTrace(global);
#endif

#if defined HAVE_PERFCTR
  InitPerfCounters(global);
#endif

  f->Call(global, 1, args);

  if (try_catch.HasCaught())  {
    FatalException(try_catch);
  }
}

static void PrintHelp();

static void ParseDebugOpt(const char* arg) {
  const char *p = 0;

  use_debug_agent = true;
  if (!strcmp (arg, "--debug-brk")) {
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
  if (p && debug_port > 1024 && debug_port <  65536)
      return;

  fprintf(stderr, "Bad debug option.\n");
  if (p) fprintf(stderr, "Debug port must be in range 1025 to 65535.\n");

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

// Parse node command line arguments.
static void ParseArgs(int argc, char **argv) {
  int i;

  // TODO use parse opts
  for (i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (strstr(arg, "--debug") == arg) {
      ParseDebugOpt(arg);
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strstr(arg, "--max-stack-size=") == arg) {
      const char *p = 0;
      p = 1 + strchr(arg, '=');
      max_stack_size = atoi(p);
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0   ||
               strcmp(arg, "-e") == 0       ||
               strcmp(arg, "--print") == 0  ||
               strcmp(arg, "-pe") == 0      ||
               strcmp(arg, "-p") == 0) {
      bool is_eval = strchr(arg, 'e') != NULL;
      bool is_print = strchr(arg, 'p') != NULL;

      // argument to -p and --print is optional
      if (is_eval == true && i + 1 >= argc) {
        fprintf(stderr, "Error: %s requires an argument\n", arg);
        exit(13);
      }

      print_eval = print_eval || is_print;
      argv[i] = const_cast<char*>("");

      // --eval, -e and -pe always require an argument
      if (is_eval == true) {
        eval_string = argv[++i];
        continue;
      }

      // next arg is the expression to evaluate unless it starts with:
      //  - a dash, then it's another switch
      //  - "\\-", then it's an escaped expression, drop the backslash
      if (argv[i + 1] == NULL) continue;
      if (argv[i + 1][0] == '-') continue;
      eval_string = argv[++i];
      if (strncmp(eval_string, "\\-", 2) == 0) ++eval_string;
    } else if (strcmp(arg, "--interactive") == 0 || strcmp(arg, "-i") == 0) {
      force_repl = true;
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = const_cast<char*>("--help");
    } else if (strcmp(arg, "--no-deprecation") == 0) {
      argv[i] = const_cast<char*>("");
      no_deprecation = true;
    } else if (strcmp(arg, "--trace-deprecation") == 0) {
      argv[i] = const_cast<char*>("");
      trace_deprecation = true;
    } else if (strcmp(arg, "--throw-deprecation") == 0) {
      argv[i] = const_cast<char*>("");
      throw_deprecation = true;
    } else if (argv[i][0] != '-') {
      break;
    }
  }

  option_end_index = i;
}


// Called from the main thread.
static void DispatchDebugMessagesAsyncCallback(uv_async_t* handle, int status) {
  v8::Debug::ProcessDebugMessages();
}


// Called from V8 Debug Agent TCP thread.
static void DispatchMessagesDebugAgentCallback() {
  uv_async_send(&dispatch_debug_messages_async);
}


static void EnableDebug(bool wait_connect) {
  // If we're called from another thread, make sure to enter the right
  // v8 isolate.
  node_isolate->Enter();

  v8::Debug::SetDebugMessageDispatchHandler(DispatchMessagesDebugAgentCallback,
                                            false);

  uv_async_init(uv_default_loop(),
                &dispatch_debug_messages_async,
                DispatchDebugMessagesAsyncCallback);
  uv_unref((uv_handle_t*) &dispatch_debug_messages_async);

  // Start the debug thread and it's associated TCP server on port 5858.
  bool r = v8::Debug::EnableAgent("node " NODE_VERSION,
                                  debug_port,
                                  wait_connect);

  // Crappy check that everything went well. FIXME
  assert(r);

  // Print out some information.
  fprintf(stderr, "debugger listening on port %d\n", debug_port);
  fflush(stderr);

  debugger_running = true;

  node_isolate->Exit();
}


#ifdef __POSIX__
static void EnableDebugSignalHandler(uv_signal_t* handle, int) {
  // Break once process will return execution to v8
  v8::Debug::DebugBreak(node_isolate);

  if (!debugger_running) {
    fprintf(stderr, "Hit SIGUSR1 - starting debugger agent.\n");
    EnableDebug(false);
  }
}


static void RegisterSignalHandler(int signal, void (*handler)(int)) {
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  sigaction(signal, &sa, NULL);
}


Handle<Value> DebugProcess(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 1) {
    return ThrowException(Exception::Error(
        String::New("Invalid number of arguments.")));
  }

  pid_t pid;
  int r;

  pid = args[0]->IntegerValue();
  r = kill(pid, SIGUSR1);
  if (r != 0) {
    return ThrowException(ErrnoException(errno, "kill"));
  }

  return Undefined();
}
#endif // __POSIX__


#ifdef _WIN32
DWORD WINAPI EnableDebugThreadProc(void* arg) {
  // Break once process will return execution to v8
  if (!debugger_running) {
    for (int i = 0; i < 1; i++) {
      fprintf(stderr, "Starting debugger agent.\r\n");
      fflush(stderr);
      EnableDebug(false);
    }
  }

  v8::Debug::DebugBreak();

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

  UnmapViewOfFile((void*) handler);

  return 0;
}


static Handle<Value> DebugProcess(const Arguments& args) {
  HandleScope scope;
  Handle<Value> rv = Undefined();
  DWORD pid;
  HANDLE process = NULL;
  HANDLE thread = NULL;
  HANDLE mapping = NULL;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = NULL;

  if (args.Length() != 1) {
    rv = ThrowException(Exception::Error(String::New("Invalid number of arguments.")));
    goto out;
  }

  pid = (DWORD) args[0]->IntegerValue();

  process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                            PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                            PROCESS_VM_READ,
                        FALSE,
                        pid);
  if (process == NULL) {
    rv = ThrowException(WinapiErrnoException(GetLastError(), "OpenProcess"));
    goto out;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       ARRAY_SIZE(mapping_name)) < 0) {
    rv = ThrowException(ErrnoException(errno, "sprintf"));
    goto out;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == NULL) {
    rv = ThrowException(WinapiErrnoException(GetLastError(),
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
    rv = ThrowException(WinapiErrnoException(GetLastError(), "MapViewOfFile"));
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
    rv = ThrowException(WinapiErrnoException(GetLastError(),
                                             "CreateRemoteThread"));
    goto out;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    rv = ThrowException(WinapiErrnoException(GetLastError(),
                                             "WaitForSingleObject"));
    goto out;
  }

 out:
  if (process != NULL) {
   CloseHandle(process);
  }
  if (thread != NULL) {
    CloseHandle(thread);
  }
  if (handler != NULL) {
    UnmapViewOfFile(handler);
  }
  if (mapping != NULL) {
    CloseHandle(mapping);
  }

  return Undefined();
}
#endif // _WIN32


static Handle<Value> DebugPause(const Arguments& args) {
  v8::Debug::DebugBreak(node_isolate);
  return Undefined();
}


static Handle<Value> DebugEnd(const Arguments& args) {
  if (debugger_running) {
    v8::Debug::DisableAgent();
    debugger_running = false;
  }

  return Undefined();
}


char** Init(int argc, char *argv[]) {
  // Initialize prog_start_time to get relative uptime.
  uv_uptime(&prog_start_time);

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // Parse a few arguments which are specific to Node.
  node::ParseArgs(argc, argv);
  // Parse the rest of the args (up to the 'option_end_index' (where '--' was
  // in the command line))
  int v8argc = option_end_index;
  char **v8argv = argv;

  if (debug_wait_connect) {
    // v8argv is a copy of argv up to the script file argument +2 if --debug-brk
    // to expose the v8 debugger js object so that node.js can set
    // a breakpoint on the first line of the startup script
    v8argc += 2;
    v8argv = new char*[v8argc];
    memcpy(v8argv, argv, sizeof(*argv) * option_end_index);
    v8argv[option_end_index] = const_cast<char*>("--expose_debug_as");
    v8argv[option_end_index + 1] = const_cast<char*>("v8debug");
  }

  // For the normal stack which moves from high to low addresses when frames
  // are pushed, we can compute the limit as stack_size bytes below the
  // the address of a stack variable (e.g. &stack_var) as an approximation
  // of the start of the stack (we're assuming that we haven't pushed a lot
  // of frames yet).
  if (max_stack_size != 0) {
    uint32_t stack_var;
    ResourceConstraints constraints;

    uint32_t *stack_limit = &stack_var - (max_stack_size / sizeof(uint32_t));
    constraints.set_stack_limit(stack_limit);
    SetResourceConstraints(&constraints); // Must be done before V8::Initialize
  }
  V8::SetFlagsFromCommandLine(&v8argc, v8argv, false);

#ifdef __POSIX__
  // Ignore SIGPIPE
  RegisterSignalHandler(SIGPIPE, SIG_IGN);
  RegisterSignalHandler(SIGINT, SignalExit);
  RegisterSignalHandler(SIGTERM, SignalExit);
#endif // __POSIX__

  uv_idle_init(uv_default_loop(), &tick_spinner);

  uv_check_init(uv_default_loop(), &check_immediate_watcher);
  uv_unref((uv_handle_t*) &check_immediate_watcher);
  uv_idle_init(uv_default_loop(), &idle_immediate_dummy);

  V8::SetFatalErrorHandler(node::OnFatalError);

  // Fetch a reference to the main isolate, so we have a reference to it
  // even when we need it to access it from another (debugger) thread.
  node_isolate = Isolate::GetCurrent();

  // If the --debug flag was specified then initialize the debug thread.
  if (use_debug_agent) {
    EnableDebug(debug_wait_connect);
  } else {
#ifdef _WIN32
    RegisterDebugSignalHandler();
#else // Posix
    static uv_signal_t signal_watcher;
    uv_signal_init(uv_default_loop(), &signal_watcher);
    uv_signal_start(&signal_watcher, EnableDebugSignalHandler, SIGUSR1);
    uv_unref((uv_handle_t*)&signal_watcher);
#endif // __POSIX__
  }

  return argv;
}


struct AtExitCallback {
  AtExitCallback* next_;
  void (*cb_)(void* arg);
  void* arg_;
};

static AtExitCallback* at_exit_functions_;


void RunAtExit() {
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


void EmitExit(v8::Handle<v8::Object> process_l) {
  // process.emit('exit')
  process_l->Set(String::NewSymbol("_exiting"), True());
  Local<Value> emit_v = process_l->Get(String::New("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit = Local<Function>::Cast(emit_v);
  Local<Value> args[] = { String::New("exit"), Integer::New(0) };
  TryCatch try_catch;
  emit->Call(process_l, 2, args);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

static char **copy_argv(int argc, char **argv) {
  size_t strlen_sum;
  char **argv_copy;
  char *argv_data;
  size_t len;
  int i;

  strlen_sum = 0;
  for(i = 0; i < argc; i++) {
    strlen_sum += strlen(argv[i]) + 1;
  }

  argv_copy = (char **) malloc(sizeof(char *) * (argc + 1) + strlen_sum);
  if (!argv_copy) {
    return NULL;
  }

  argv_data = (char *) argv_copy + sizeof(char *) * (argc + 1);

  for(i = 0; i < argc; i++) {
    argv_copy[i] = argv_data;
    len = strlen(argv[i]) + 1;
    memcpy(argv_data, argv[i], len);
    argv_data += len;
  }

  argv_copy[argc] = NULL;

  return argv_copy;
}

int Start(int argc, char *argv[]) {
  // Hack aroung with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  // Logic to duplicate argv as Init() modifies arguments
  // that are passed into it.
  char **argv_copy = copy_argv(argc, argv);

  // This needs to run *before* V8::Initialize()
  // Use copy here as to not modify the original argv:
  Init(argc, argv_copy);

  V8::Initialize();
  {
    Locker locker;
    HandleScope handle_scope;

    // Create the one and only Context.
    Persistent<Context> context = Context::New();
    Context::Scope context_scope(context);

    // Use original argv, as we're just copying values out of it.
    Handle<Object> process_l = SetupProcessObject(argc, argv);
    v8_typed_array::AttachBindings(context->Global());

    // Create all the objects, load modules, do everything.
    // so your next reading stop should be node::Load()!
    Load(process_l);

    // All our arguments are loaded. We've evaluated all of the scripts. We
    // might even have created TCP servers. Now we enter the main eventloop. If
    // there are no watchers on the loop (except for the ones that were
    // uv_unref'd) then this function exits. As long as there are active
    // watchers, it blocks.
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    EmitExit(process_l);
    RunAtExit();

#ifndef NDEBUG
    context.Dispose();
#endif
  }

#ifndef NDEBUG
  // Clean up. Not strictly necessary.
  V8::Dispose();
#endif  // NDEBUG

  // Clean up the copy:
  free(argv_copy);

  return 0;
}


}  // namespace node

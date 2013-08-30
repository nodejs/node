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

#if defined HAVE_DTRACE || defined HAVE_ETW || defined HAVE_SYSTEMTAP
#include "node_dtrace.h"
#endif

#if HAVE_SYSTEMTAP
#include "node_provider.h"
#endif

#include "ares.h"
#include "handle_wrap.h"
#include "req_wrap.h"
#include "string_bytes.h"
#include "uv.h"
#include "v8-debug.h"
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
using v8::Persistent;
using v8::PropertyCallbackInfo;
using v8::ResourceConstraints;
using v8::SetResourceConstraints;
using v8::String;
using v8::ThrowException;
using v8::TryCatch;
using v8::Uint32;
using v8::V8;
using v8::Value;
using v8::kExternalUnsignedIntArray;

QUEUE handle_wrap_queue = { &handle_wrap_queue, &handle_wrap_queue };
QUEUE req_wrap_queue = { &req_wrap_queue, &req_wrap_queue };

// declared in req_wrap.h
Cached<String> process_symbol;
Cached<String> domain_symbol;

// declared in node_internals.h
Persistent<Object> process_p;

static Persistent<Function> process_tickCallback;
static Persistent<Object> binding_cache;
static Persistent<Array> module_load_list;
static Persistent<Array> p_domain_box;

static Cached<String> exports_symbol;

static Cached<String> errno_symbol;
static Cached<String> syscall_symbol;
static Cached<String> errpath_symbol;
static Cached<String> code_symbol;

static Cached<String> rss_symbol;
static Cached<String> heap_total_symbol;
static Cached<String> heap_used_symbol;

static Cached<String> fatal_exception_symbol;

static Cached<String> enter_symbol;
static Cached<String> exit_symbol;
static Cached<String> disposed_symbol;

// Essential for node_wrap.h
Persistent<FunctionTemplate> pipeConstructorTmpl;
Persistent<FunctionTemplate> tcpConstructorTmpl;
Persistent<FunctionTemplate> ttyConstructorTmpl;

static bool print_eval = false;
static bool force_repl = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static char *eval_string = NULL;
static int option_end_index = 0;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static int debug_port = 5858;
bool using_domains = false;

// used by C++ modules as well
bool no_deprecation = false;

static uv_check_t check_immediate_watcher;
static uv_idle_t idle_immediate_dummy;
static bool need_immediate_cb;
static Cached<String> immediate_callback_sym;

// for quick ref to tickCallback values
static struct {
  uint32_t length;
  uint32_t index;
  uint32_t in_tick;
  uint32_t last_threw;
} tick_infobox;

// easily communicate domain depth
static struct {
  uint32_t count;
} domain_flag;

// process-relative uptime base, initialized at start-up
static double prog_start_time;

static volatile bool debugger_running = false;
static uv_async_t dispatch_debug_messages_async;
static uv_async_t emit_debug_enabled_async;

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
  virtual void Free(void* data);
 private:
  ArrayBufferAllocator() {}
  ArrayBufferAllocator(const ArrayBufferAllocator&);
  void operator=(const ArrayBufferAllocator&);
};

ArrayBufferAllocator ArrayBufferAllocator::the_singleton;


void* ArrayBufferAllocator::Allocate(size_t length) {
  if (length > kMaxLength) return NULL;
  return new char[length];
}


void ArrayBufferAllocator::Free(void* data) {
  delete[] static_cast<char*>(data);
}


static void CheckImmediate(uv_check_t* handle, int status) {
  assert(handle == &check_immediate_watcher);
  assert(status == 0);

  HandleScope scope(node_isolate);

  if (immediate_callback_sym.IsEmpty()) {
    immediate_callback_sym =
        FIXED_ONE_BYTE_STRING(node_isolate, "_immediateCallback");
  }

  MakeCallback(process_p, immediate_callback_sym, 0, NULL);
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
  Local<String> estring = OneByteString(node_isolate, errno_string(errorno));
  if (msg == NULL || msg[0] == '\0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(node_isolate, msg);

  Local<String> cons1 =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(node_isolate, ", "));
  Local<String> cons2 = String::Concat(cons1, message);

  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "syscall");
    errno_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "errno");
    errpath_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "path");
    code_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "code");
  }

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

  obj->Set(errno_symbol, Integer::New(errorno, node_isolate));
  obj->Set(code_symbol, estring);
  if (path) obj->Set(errpath_symbol, String::NewFromUtf8(node_isolate, path));
  if (syscall) obj->Set(syscall_symbol, OneByteString(node_isolate, syscall));
  return e;
}


// hack alert! copy of ErrnoException, tuned for uv errors
Local<Value> UVException(int errorno,
                         const char *syscall,
                         const char *msg,
                         const char *path) {
  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "syscall");
    errno_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "errno");
    errpath_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "path");
    code_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "code");
  }

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
  obj->Set(errno_symbol, Integer::New(errorno, node_isolate));
  obj->Set(code_symbol, estring);
  if (path) obj->Set(errpath_symbol, path_str);
  if (syscall) obj->Set(syscall_symbol, OneByteString(node_isolate, syscall));
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
  Local<String> message = OneByteString(node_isolate, msg);

  if (syscall_symbol.IsEmpty()) {
    syscall_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "syscall");
    errno_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "errno");
    errpath_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "path");
    code_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "code");
  }

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

  obj->Set(errno_symbol, Integer::New(errorno, node_isolate));
  if (path) obj->Set(errpath_symbol, String::NewFromUtf8(node_isolate, path));
  if (syscall) obj->Set(syscall_symbol, OneByteString(node_isolate, syscall));
  return e;
}
#endif


void SetupDomainUse(const FunctionCallbackInfo<Value>& args) {
  if (using_domains) return;
  HandleScope scope(node_isolate);
  using_domains = true;
  Local<Object> process = PersistentToLocal(node_isolate, process_p);
  Local<Value> tdc_v =
      process->Get(FIXED_ONE_BYTE_STRING(node_isolate, "_tickDomainCallback"));
  if (!tdc_v->IsFunction()) {
    fprintf(stderr, "process._tickDomainCallback assigned to non-function\n");
    abort();
  }
  Local<Function> tdc = tdc_v.As<Function>();
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_tickCallback"), tdc);
  process_tickCallback.Reset(node_isolate, tdc);
  if (!args[0]->IsArray()) {
    fprintf(stderr, "_setupDomainUse first argument must be an array\n");
    abort();
  }
  p_domain_box.Reset(node_isolate, args[0].As<Array>());
  if (!args[1]->IsObject()) {
    fprintf(stderr, "_setupDomainUse second argument must be an object\n");
    abort();
  }
  Local<Object> flag = args[1].As<Object>();
  flag->SetIndexedPropertiesToExternalArrayData(&domain_flag,
                                                kExternalUnsignedIntArray,
                                                1);
}


bool InDomain() {
  return using_domains && domain_flag.count > 0;
}


Handle<Value> GetDomain() {
  // no domain can exist if no domain module has been loaded
  if (!InDomain() || p_domain_box.IsEmpty())
    return Null(node_isolate);

  return PersistentToLocal(node_isolate, p_domain_box)->Get(0);
}


Handle<Value>
MakeDomainCallback(const Handle<Object> object,
                   const Handle<Function> callback,
                   int argc,
                   Handle<Value> argv[]) {
  // TODO(trevnorris) Hook for long stack traces to be made here.

  // lazy load domain specific symbols
  if (enter_symbol.IsEmpty()) {
    enter_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "enter");
    exit_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "exit");
    disposed_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "_disposed");
  }

  Local<Value> domain_v = object->Get(domain_symbol);
  Local<Object> domain;
  Local<Function> enter;
  Local<Function> exit;

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  bool has_domain = domain_v->IsObject();
  if (has_domain) {
    domain = domain_v->ToObject();
    assert(!domain.IsEmpty());
    if (domain->Get(disposed_symbol)->IsTrue()) {
      // domain has been disposed of.
      return Undefined(node_isolate);
    }
    enter = Local<Function>::Cast(domain->Get(enter_symbol));
    assert(!enter.IsEmpty());
    enter->Call(domain, 0, NULL);

    if (try_catch.HasCaught()) {
      return Undefined(node_isolate);
    }
  }

  Local<Value> ret = callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(node_isolate);
  }

  if (has_domain) {
    exit = Local<Function>::Cast(domain->Get(exit_symbol));
    assert(!exit.IsEmpty());
    exit->Call(domain, 0, NULL);

    if (try_catch.HasCaught()) {
      return Undefined(node_isolate);
    }
  }

  if (tick_infobox.last_threw == 1) {
    tick_infobox.last_threw = 0;
    return ret;
  }

  if (tick_infobox.in_tick == 1) {
    return ret;
  }

  if (tick_infobox.length == 0) {
    tick_infobox.index = 0;
    return ret;
  }

  // process nextTicks after call
  Local<Object> process = PersistentToLocal(node_isolate, process_p);
  Local<Function> fn = PersistentToLocal(node_isolate, process_tickCallback);
  fn->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    return Undefined(node_isolate);
  }

  return ret;
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const Handle<Function> callback,
             int argc,
             Handle<Value> argv[]) {
  // TODO(trevnorris) Hook for long stack traces to be made here.
  Local<Object> process = PersistentToLocal(node_isolate, process_p);

  if (using_domains)
    return MakeDomainCallback(object, callback, argc, argv);

  // lazy load no domain next tick callbacks
  if (process_tickCallback.IsEmpty()) {
    Local<Value> cb_v =
        process->Get(FIXED_ONE_BYTE_STRING(node_isolate, "_tickCallback"));
    if (!cb_v->IsFunction()) {
      fprintf(stderr, "process._tickCallback assigned to non-function\n");
      abort();
    }
    process_tickCallback.Reset(node_isolate, cb_v.As<Function>());
  }

  TryCatch try_catch;
  try_catch.SetVerbose(true);

  Local<Value> ret = callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    return Undefined(node_isolate);
  }

  if (tick_infobox.in_tick == 1) {
    return ret;
  }

  if (tick_infobox.length == 0) {
    tick_infobox.index = 0;
    return ret;
  }

  // process nextTicks after call
  Local<Function> fn = PersistentToLocal(node_isolate, process_tickCallback);
  fn->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    return Undefined(node_isolate);
  }

  return ret;
}


// Internal only.
Handle<Value>
MakeCallback(const Handle<Object> object,
             uint32_t index,
             int argc,
             Handle<Value> argv[]) {
  HandleScope scope(node_isolate);

  Local<Function> callback = object->Get(index).As<Function>();
  assert(callback->IsFunction());

  if (using_domains)
    return scope.Close(MakeDomainCallback(object, callback, argc, argv));
  return scope.Close(MakeCallback(object, callback, argc, argv));
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const Handle<String> symbol,
             int argc,
             Handle<Value> argv[]) {
  HandleScope scope(node_isolate);

  Local<Function> callback = object->Get(symbol).As<Function>();
  assert(callback->IsFunction());

  if (using_domains)
    return scope.Close(MakeDomainCallback(object, callback, argc, argv));
  return scope.Close(MakeCallback(object, callback, argc, argv));
}


Handle<Value>
MakeCallback(const Handle<Object> object,
             const char* method,
             int argc,
             Handle<Value> argv[]) {
  HandleScope scope(node_isolate);

  Local<String> method_string = OneByteString(node_isolate, method);
  Handle<Value> ret = MakeCallback(object, method_string, argc, argv);

  return scope.Close(ret);
}


enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope(node_isolate);

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

  if (displayed_error) return;
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
    if (w->persistent().IsEmpty()) continue;
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
    if (w->persistent().IsEmpty() || (w->flags_ & HandleWrap::kUnref)) continue;
    Local<Object> object = w->object();
    Local<Value> owner = object->Get(owner_sym);
    if (owner->IsUndefined()) owner = object;
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
  args.GetReturnValue().Set(getuid());
}


static void GetGid(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(getgid());
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
    if (groups[i] == egid) seen_egid = true;
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
    if (must_free) free(user);
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
  if (uv_uptime(&uptime)) return;
  args.GetReturnValue().Set(uptime - prog_start_time);
}


void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  size_t rss;

  int err = uv_resident_set_memory(&rss);
  if (err) {
    return ThrowUVException(err, "uv_resident_set_memory");
  }

  Local<Object> info = Object::New();

  if (rss_symbol.IsEmpty()) {
    rss_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "rss");
    heap_total_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "heapTotal");
    heap_used_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "heapUsed");
  }

  info->Set(rss_symbol, Number::New(rss));

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  node_isolate->GetHeapStatistics(&v8_heap_stats);
  info->Set(heap_total_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.total_heap_size(),
                                     node_isolate));
  info->Set(heap_used_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.used_heap_size(),
                                     node_isolate));

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
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  char symbol[1024], *base, *pos;
  uv_lib_t lib;
  int r;

  if (args.Length() < 2) {
    return ThrowError("process.dlopen takes exactly 2 arguments.");
  }

  Local<Object> module = args[0]->ToObject();  // Cast
  String::Utf8Value filename(args[1]);  // Cast

  if (exports_symbol.IsEmpty()) {
    exports_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "exports");
  }
  Local<Object> exports = module->Get(exports_symbol)->ToObject();

  if (uv_dlopen(*filename, &lib)) {
    Local<String> errmsg = OneByteString(node_isolate, uv_dlerror(&lib));
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

  if (fatal_exception_symbol.IsEmpty()) {
    fatal_exception_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "_fatalException");
  }

  Local<Object> process = PersistentToLocal(node_isolate, process_p);
  Local<Value> fatal_v = process->Get(fatal_exception_symbol);

  if (!fatal_v->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(error, message);
    exit(6);
  }

  Local<Function> fatal_f = Local<Function>::Cast(fatal_v);

  TryCatch fatal_try_catch;

  // Do not call FatalException when _fatalException handler throws
  fatal_try_catch.SetVerbose(false);

  // this will return true if the JS layer handled it, false otherwise
  Local<Value> caught = fatal_f->Call(process, 1, &error);

  if (fatal_try_catch.HasCaught()) {
    // the fatal exception function threw, so we must exit
    ReportException(fatal_try_catch);
    exit(7);
  }

  if (false == caught->BooleanValue()) {
    ReportException(error, message);
    exit(8);
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
  HandleScope scope(node_isolate);

  Local<String> module = args[0]->ToString();
  String::Utf8Value module_v(module);
  node_module_struct* modp;

  Local<Object> cache = PersistentToLocal(node_isolate, binding_cache);
  Local<Object> exports;

  if (cache->Has(module)) {
    exports = cache->Get(module)->ToObject();
    args.GetReturnValue().Set(exports);
    return;
  }

  // Append a string to process.moduleLoadList
  char buf[1024];
  snprintf(buf, sizeof(buf), "Binding %s", *module_v);

  Local<Array> modules = PersistentToLocal(node_isolate, module_load_list);
  uint32_t l = modules->Length();
  modules->Set(l, OneByteString(node_isolate, buf));

  if ((modp = get_builtin_module(*module_v)) != NULL) {
    exports = Object::New();
    // Internal bindings don't have a "module" object,
    // only exports.
    modp->register_func(exports, Undefined(node_isolate));
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
  if (getenv(*key)) rc = 0;
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
  if (rc != -1) info.GetReturnValue().Set(rc);
}


static void EnvDeleter(Local<String> property,
                       const PropertyCallbackInfo<Boolean>& info) {
  HandleScope scope(node_isolate);
  bool rc = true;
#ifdef __POSIX__
  String::Utf8Value key(property);
  rc = getenv(*key) != NULL;
  if (rc) unsetenv(*key);
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
  while (environ[size]) size++;

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
  if (environment == NULL) return;  // This should not happen.
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
  info.GetReturnValue().Set(need_immediate_cb);
}


static void NeedImmediateCallbackSetter(Local<String> property,
                                        Local<Value> value,
                                        const PropertyCallbackInfo<void>&) {
  HandleScope scope(node_isolate);

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


#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->Set(OneByteString(node_isolate, str), var, v8::ReadOnly);            \
  } while (0)


Handle<Object> SetupProcessObject(int argc, char *argv[]) {
  HandleScope scope(node_isolate);

  int i, j;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();
  process_template->SetClassName(
      FIXED_ONE_BYTE_STRING(node_isolate, "process"));

  Local<Object> process = process_template->GetFunction()->NewInstance();
  assert(process.IsEmpty() == false);
  assert(process->IsObject() == true);

  process_p.Reset(node_isolate, process);

  process->SetAccessor(FIXED_ONE_BYTE_STRING(node_isolate, "title"),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

  // process.version
  READONLY_PROPERTY(process,
                    "version",
                    FIXED_ONE_BYTE_STRING(node_isolate, NODE_VERSION));

  // process.moduleLoadList
  Local<Array> modules = Array::New();
  module_load_list.Reset(node_isolate, modules);
  READONLY_PROPERTY(process, "moduleLoadList", modules);

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
  READONLY_PROPERTY(
      versions,
      "openssl",
      OneByteString(node_isolate, &OPENSSL_VERSION_TEXT[i], j - i));
#endif



  // process.arch
  READONLY_PROPERTY(process, "arch", OneByteString(node_isolate, ARCH));

  // process.platform
  READONLY_PROPERTY(process,
                    "platform",
                    OneByteString(node_isolate, PLATFORM));

  // process.argv
  Local<Array> arguments = Array::New(argc - option_end_index + 1);
  arguments->Set(0, String::NewFromUtf8(node_isolate, argv[0]));
  for (j = 1, i = option_end_index; i < argc; j++, i++) {
    arguments->Set(j, String::NewFromUtf8(node_isolate, argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "argv"), arguments);

  // process.execArgv
  Local<Array> exec_argv = Array::New(option_end_index - 1);
  for (j = 1, i = 0; j < option_end_index; j++, i++) {
    exec_argv->Set(i, String::NewFromUtf8(node_isolate, argv[j]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "execArgv"), exec_argv);

  // create process.env
  Local<ObjectTemplate> envTemplate = ObjectTemplate::New();
  envTemplate->SetNamedPropertyHandler(EnvGetter,
                                       EnvSetter,
                                       EnvQuery,
                                       EnvDeleter,
                                       EnvEnumerator,
                                       Object::New());
  Local<Object> env = envTemplate->NewInstance();
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "env"), env);

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

  NODE_SET_METHOD(process, "_setupDomainUse", SetupDomainUse);

  // values use to cross communicate with processNextTick
  Local<Object> info_box = Object::New();
  info_box->SetIndexedPropertiesToExternalArrayData(&tick_infobox,
                                                    kExternalUnsignedIntArray,
                                                    4);
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_tickInfoBox"), info_box);

  // pre-set _events object for faster emit checks
  process->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_events"), Object::New());

  return scope.Close(process);
}


#undef READONLY_PROPERTY


static void AtExit() {
  uv_tty_reset_mode();
}


static void SignalExit(int signal) {
  uv_tty_reset_mode();
  _exit(128 + signal);
}


void Load(Handle<Object> process_l) {
  HandleScope handle_scope(node_isolate);

  process_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "process");
  domain_symbol = FIXED_ONE_BYTE_STRING(node_isolate, "domain");

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
  Local<Object> global = v8::Context::GetCurrent()->Global();

#if defined HAVE_DTRACE || defined HAVE_ETW || defined HAVE_SYSTEMTAP
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

  Local<Value> arg = process_l;
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

  // TODO(bnoordhuis) use parse opts
  for (i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (strstr(arg, "--debug") == arg) {
      ParseDebugOpt(arg);
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
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


// Called from the main thread
static void EmitDebugEnabledAsyncCallback(uv_async_t* handle, int status) {
  HandleScope handle_scope(node_isolate);
  Local<Object> obj = Object::New();
  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "cmd"),
           FIXED_ONE_BYTE_STRING(node_isolate, "NODE_DEBUG_ENABLED"));
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(node_isolate, "internalMessage"),
    obj
  };
  MakeCallback(process_p, "emit", ARRAY_SIZE(args), args);
}


// Called from the signal watcher callback
static void EmitDebugEnabled() {
  uv_async_send(&emit_debug_enabled_async);
}


static void EnableDebug(bool wait_connect) {
  // If we're called from another thread, make sure to enter the right
  // v8 isolate.
  node_isolate->Enter();

  v8::Debug::SetDebugMessageDispatchHandler(DispatchMessagesDebugAgentCallback,
                                            false);

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

  // Do not emit NODE_DEBUG_ENABLED when debugger is enabled before starting
  // the main process (i.e. when called via `node --debug`)
  if (!process_p.IsEmpty())
    EmitDebugEnabled();

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
#endif  // __POSIX__


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

  v8::Debug::DebugBreak(node_isolate);

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


char** Init(int argc, char *argv[]) {
  // Initialize prog_start_time to get relative uptime.
  uv_uptime(&prog_start_time);

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // init async debug messages dispatching
  uv_async_init(uv_default_loop(),
                &dispatch_debug_messages_async,
                DispatchDebugMessagesAsyncCallback);
  uv_unref(reinterpret_cast<uv_handle_t*>(&dispatch_debug_messages_async));

  // init async NODE_DEBUG_ENABLED emitter
  uv_async_init(uv_default_loop(),
                &emit_debug_enabled_async,
                EmitDebugEnabledAsyncCallback);
  uv_unref(reinterpret_cast<uv_handle_t*>(&emit_debug_enabled_async));

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

  V8::SetFlagsFromCommandLine(&v8argc, v8argv, false);

  const char typed_arrays_flag[] = "--harmony_typed_arrays";
  V8::SetFlagsFromString(typed_arrays_flag, sizeof(typed_arrays_flag) - 1);
  V8::SetArrayBufferAllocator(&ArrayBufferAllocator::the_singleton);

  // Fetch a reference to the main isolate, so we have a reference to it
  // even when we need it to access it from another (debugger) thread.
  node_isolate = Isolate::GetCurrent();

#ifdef __POSIX__
  // Ignore SIGPIPE
  RegisterSignalHandler(SIGPIPE, SIG_IGN);
  RegisterSignalHandler(SIGINT, SignalExit);
  RegisterSignalHandler(SIGTERM, SignalExit);
#endif  // __POSIX__

  uv_check_init(uv_default_loop(), &check_immediate_watcher);
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_immediate_watcher));
  uv_idle_init(uv_default_loop(), &idle_immediate_dummy);

  V8::SetFatalErrorHandler(node::OnFatalError);
  V8::AddMessageListener(OnMessage);

  // If the --debug flag was specified then initialize the debug thread.
  if (use_debug_agent) {
    EnableDebug(debug_wait_connect);
  } else {
#ifdef _WIN32
    RegisterDebugSignalHandler();
#else  // Posix
    static uv_signal_t signal_watcher;
    uv_signal_init(uv_default_loop(), &signal_watcher);
    uv_signal_start(&signal_watcher, EnableDebugSignalHandler, SIGUSR1);
    uv_unref(reinterpret_cast<uv_handle_t*>(&signal_watcher));
#endif  // __POSIX__
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
  process_l->Set(FIXED_ONE_BYTE_STRING(node_isolate, "_exiting"),
                 True(node_isolate));
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(node_isolate, "exit"),
    Integer::New(0, node_isolate)
  };
  MakeCallback(process_l, "emit", ARRAY_SIZE(args), args);
}

static char **copy_argv(int argc, char **argv) {
  size_t strlen_sum;
  char **argv_copy;
  char *argv_data;
  size_t len;
  int i;

  strlen_sum = 0;
  for (i = 0; i < argc; i++) {
    strlen_sum += strlen(argv[i]) + 1;
  }

  argv_copy = static_cast<char**>(
      malloc(sizeof(*argv_copy) * (argc + 1) + strlen_sum));
  if (!argv_copy) {
    return NULL;
  }

  argv_data = reinterpret_cast<char*>(argv_copy) +
              sizeof(*argv_copy) * (argc + 1);

  for (i = 0; i < argc; i++) {
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
    Locker locker(node_isolate);
    HandleScope handle_scope(node_isolate);

    // Create the one and only Context.
    Local<Context> context = Context::New(node_isolate);
    Context::Scope context_scope(context);

    binding_cache.Reset(node_isolate, Object::New());

    // Use original argv, as we're just copying values out of it.
    Local<Object> process_l = SetupProcessObject(argc, argv);

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

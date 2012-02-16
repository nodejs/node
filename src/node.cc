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

#include <node.h>

#include <uv.h>

#include <v8-debug.h>
#ifdef HAVE_DTRACE
# include <node_dtrace.h>
#endif

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#if defined(_MSC_VER)
#define snprintf _snprintf
#endif
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

#if defined(__MINGW32__) || defined(_MSC_VER)
# include <platform_win32.h> /* winapi_perror() */
#endif

#ifdef __POSIX__
# include <pwd.h> /* getpwnam() */
# include <grp.h> /* getgrnam() */
#endif

#include "platform.h"
#include <node_buffer.h>
#ifdef __POSIX__
# include <node_io_watcher.h>
#endif
#include <node_file.h>
#include <node_http_parser.h>
#ifdef __POSIX__
# include <node_signal_watcher.h>
# include <node_stat_watcher.h>
#endif
#include <node_constants.h>
#include <node_javascript.h>
#include <node_version.h>
#include <node_string.h>
#if HAVE_OPENSSL
# include <node_crypto.h>
#endif
#include <node_script.h>
#include <v8_typed_array.h>

using namespace v8;

# ifdef __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
# elif !defined(_MSC_VER)
extern char **environ;
# endif

namespace node {

static Persistent<Object> process;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> errpath_symbol;
static Persistent<String> code_symbol;

static Persistent<String> rss_symbol;
static Persistent<String> heap_total_symbol;
static Persistent<String> heap_used_symbol;

static Persistent<String> listeners_symbol;
static Persistent<String> uncaught_exception_symbol;
static Persistent<String> emit_symbol;


static bool print_eval = false;
static char *eval_string = NULL;
static int option_end_index = 0;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static int debug_port=5858;
static int max_stack_size = 0;

static uv_check_t check_tick_watcher;
static uv_prepare_t prepare_tick_watcher;
static uv_idle_t tick_spinner;
static bool need_tick_cb;
static Persistent<String> tick_callback_sym;


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

#ifdef __POSIX__
// Buffer for getpwnam_r(), getgrpam_r() and other misc callers; keep this
// scoped at file-level rather than method-level to avoid excess stack usage.
static char getbuf[PATH_MAX + 1];
#endif

// We need to notify V8 when we're idle so that it can run the garbage
// collector. The interface to this is V8::IdleNotification(). It returns
// true if the heap hasn't be fully compacted, and needs to be run again.
// Returning false means that it doesn't have anymore work to do.
//
// A rather convoluted algorithm has been devised to determine when Node is
// idle. You'll have to figure it out for yourself.
static uv_check_t gc_check;
static uv_idle_t gc_idle;
static uv_timer_t gc_timer;
bool need_gc;


#define FAST_TICK 700.
#define GC_WAIT_TIME 5000.
#define RPM_SAMPLES 100
#define TICK_TIME(n) tick_times[(tick_time_head - (n)) % RPM_SAMPLES]
static int64_t tick_times[RPM_SAMPLES];
static int tick_time_head;

static void CheckStatus(uv_timer_t* watcher, int status);

static void StartGCTimer () {
  if (!uv_is_active((uv_handle_t*) &gc_timer)) {
    uv_timer_start(&gc_timer, node::CheckStatus, 5000, 5000);
  }
}

static void StopGCTimer () {
  if (uv_is_active((uv_handle_t*) &gc_timer)) {
    uv_timer_stop(&gc_timer);
  }
}

static void Idle(uv_idle_t* watcher, int status) {
  assert((uv_idle_t*) watcher == &gc_idle);

  if (V8::IdleNotification()) {
    uv_idle_stop(&gc_idle);
    StopGCTimer();
  }
}


// Called directly after every call to select() (or epoll, or whatever)
static void Check(uv_check_t* watcher, int status) {
  assert(watcher == &gc_check);

  tick_times[tick_time_head] = uv_now(uv_default_loop());
  tick_time_head = (tick_time_head + 1) % RPM_SAMPLES;

  StartGCTimer();

  for (int i = 0; i < (int)(GC_WAIT_TIME/FAST_TICK); i++) {
    double d = TICK_TIME(i+1) - TICK_TIME(i+2);
    //printf("d = %f\n", d);
    // If in the last 5 ticks the difference between
    // ticks was less than 0.7 seconds, then continue.
    if (d < FAST_TICK) {
      //printf("---\n");
      return;
    }
  }

  // Otherwise start the gc!

  //fprintf(stderr, "start idle 2\n");
  uv_idle_start(&gc_idle, node::Idle);
}


static void Tick(void) {
  // Avoid entering a V8 scope.
  if (!need_tick_cb) return;

  need_tick_cb = false;
  if (uv_is_active((uv_handle_t*) &tick_spinner)) {
    uv_idle_stop(&tick_spinner);
    uv_unref(uv_default_loop());
  }

  HandleScope scope;

  if (tick_callback_sym.IsEmpty()) {
    // Lazily set the symbol
    tick_callback_sym =
      Persistent<String>::New(String::NewSymbol("_tickCallback"));
  }

  Local<Value> cb_v = process->Get(tick_callback_sym);
  if (!cb_v->IsFunction()) return;
  Local<Function> cb = Local<Function>::Cast(cb_v);

  TryCatch try_catch;

  cb->Call(process, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


static void Spin(uv_idle_t* handle, int status) {
  assert((uv_idle_t*) handle == &tick_spinner);
  assert(status == 0);
  Tick();
}


static Handle<Value> NeedTickCallback(const Arguments& args) {
  HandleScope scope;
  need_tick_cb = true;
  // TODO: this tick_spinner shouldn't be necessary. An ev_prepare should be
  // sufficent, the problem is only in the case of the very last "tick" -
  // there is nothing left to do in the event loop and libev will exit. The
  // ev_prepare callback isn't called before exiting. Thus we start this
  // tick_spinner to keep the event loop alive long enough to handle it.
  if (!uv_is_active((uv_handle_t*) &tick_spinner)) {
    uv_idle_start(&tick_spinner, Spin);
    uv_ref(uv_default_loop());
  }
  return Undefined();
}


static void PrepareTick(uv_prepare_t* handle, int status) {
  assert(handle == &prepare_tick_watcher);
  assert(status == 0);
  Tick();
}


static void CheckTick(uv_check_t* handle, int status) {
  assert(handle == &check_tick_watcher);
  assert(status == 0);
  Tick();
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


Handle<Value> FromConstructorTemplate(Persistent<FunctionTemplate>& t,
                                      const Arguments& args) {
  HandleScope scope;

  const int argc = args.Length();
  Local<Value>* argv = new Local<Value>[argc];

  for (int i = 0; i < argc; ++i) {
    argv[i] = args[i];
  }

  Local<Object> instance = t->GetFunction()->NewInstance(argc, argv);

  delete[] argv;

  return scope.Close(instance);
}


// MakeCallback may only be made directly off the event loop.
// That is there can be no JavaScript stack frames underneath it.
// (Is there any way to assert that?)
//
// Maybe make this a method of a node::Handle super class
//
void MakeCallback(Handle<Object> object,
                  const char* method,
                  int argc,
                  Handle<Value> argv[]) {
  HandleScope scope;

  Local<Value> callback_v = object->Get(String::New(method));
  if (!callback_v->IsFunction()) {
    fprintf(stderr, "method = %s", method);
  }
  assert(callback_v->IsFunction());
  Local<Function> callback = Local<Function>::Cast(callback_v);

  // TODO Hook for long stack traces to be made here.

  TryCatch try_catch;

  callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


void SetErrno(uv_err_t err) {
  HandleScope scope;

  if (errno_symbol.IsEmpty()) {
    errno_symbol = NODE_PSYMBOL("errno");
  }

  if (err.code == UV_UNKNOWN) {
    char errno_buf[100];
    snprintf(errno_buf, 100, "Unknown system errno %d", err.sys_errno_);
    Context::GetCurrent()->Global()->Set(errno_symbol, String::New(errno_buf));
  } else {
    Context::GetCurrent()->Global()->Set(errno_symbol,
                                         String::NewSymbol(uv_err_name(err)));
  }
}


enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope;

  if (!encoding_v->IsString()) return _default;

  String::Utf8Value encoding(encoding_v->ToString());

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
  } else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  } else if (strcasecmp(*encoding, "hex") == 0) {
    return HEX;
  } else if (strcasecmp(*encoding, "raw") == 0) {
    fprintf(stderr, "'raw' (array of integers) has been removed. "
                    "Use 'binary'.\n");
    return BINARY;
  } else if (strcasecmp(*encoding, "raws") == 0) {
    fprintf(stderr, "'raws' encoding has been renamed to 'binary'. "
                    "Please update your code.\n");
    return BINARY;
  } else {
    return _default;
  }
}

Local<Value> Encode(const void *buf, size_t len, enum encoding encoding) {
  HandleScope scope;

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

  if (encoding == BINARY && Buffer::HasInstance(val)) {
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

  Local<String> str = val->ToString();

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

    // HACK HACK HACK
    //
    // FIXME
    //
    // Because of how CommonJS modules work, all scripts are wrapped with a
    // "function (function (exports, __filename, ...) {"
    // to provide script local variables.
    //
    // When reporting errors on the first line of a script, this wrapper
    // function is leaked to the user. This HACK is to remove it. The length
    // of the wrapper is 62. That wrapper is defined in src/node.js
    //
    // If that wrapper is ever changed, then this number also has to be
    // updated. Or - someone could clean this up so that the two peices
    // don't need to be changed.
    //
    // Even better would be to get support into V8 for wrappers that
    // shouldn't be reported to users.
    int offset = linenum == 1 ? 62 : 0;

    fprintf(stderr, "%s\n", sourceline_string + offset);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = offset; i < start; i++) {
      fprintf(stderr, " ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
  }
}


static void ReportException(TryCatch &try_catch, bool show_line) {
  HandleScope scope;
  Handle<Message> message = try_catch.Message();

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

    String::Utf8Value msg(!isErrorObject ? er->ToString()
                         : er->ToObject()->Get(String::New("message"))->ToString());
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
    exit(1);
  }

  Local<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(try_catch, true);
    exit(1);
  }

  return scope.Close(result);
}


static Handle<Value> Chdir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }

  String::Utf8Value path(args[0]->ToString());

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

static Handle<Value> GetUid(const Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);
  int uid = getuid();
  return scope.Close(Integer::New(uid));
}


static Handle<Value> GetGid(const Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);
  int gid = getgid();
  return scope.Close(Integer::New(gid));
}


static Handle<Value> SetGid(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    return ThrowException(Exception::Error(
      String::New("setgid requires 1 argument")));
  }

  int gid;

  if (args[0]->IsNumber()) {
    gid = args[0]->Int32Value();
  } else if (args[0]->IsString()) {
    String::Utf8Value grpnam(args[0]->ToString());
    struct group grp, *grpp = NULL;
    int err;

    if ((err = getgrnam_r(*grpnam, &grp, getbuf, ARRAY_SIZE(getbuf), &grpp)) ||
        grpp == NULL) {
      if (errno == 0)
        return ThrowException(Exception::Error(
          String::New("setgid group id does not exist")));
      else
        return ThrowException(ErrnoException(errno, "getgrnam_r"));
    }

    gid = grpp->gr_gid;
  } else {
    return ThrowException(Exception::Error(
      String::New("setgid argument must be a number or a string")));
  }

  int result;
  if ((result = setgid(gid)) != 0) {
    return ThrowException(ErrnoException(errno, "setgid"));
  }
  return Undefined();
}


static Handle<Value> SetUid(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    return ThrowException(Exception::Error(
          String::New("setuid requires 1 argument")));
  }

  int uid;

  if (args[0]->IsNumber()) {
    uid = args[0]->Int32Value();
  } else if (args[0]->IsString()) {
    String::Utf8Value pwnam(args[0]->ToString());
    struct passwd pwd, *pwdp = NULL;
    int err;

    if ((err = getpwnam_r(*pwnam, &pwd, getbuf, ARRAY_SIZE(getbuf), &pwdp)) ||
        pwdp == NULL) {
      if (errno == 0)
        return ThrowException(Exception::Error(
          String::New("setuid user id does not exist")));
      else
        return ThrowException(ErrnoException(errno, "getpwnam_r"));
    }

    uid = pwdp->pw_uid;
  } else {
    return ThrowException(Exception::Error(
      String::New("setuid argument must be a number or a string")));
  }

  int result;
  if ((result = setuid(uid)) != 0) {
    return ThrowException(ErrnoException(errno, "setuid"));
  }
  return Undefined();
}


#endif // __POSIX__


v8::Handle<v8::Value> Exit(const v8::Arguments& args) {
  HandleScope scope;
  exit(args[0]->IntegerValue());
  return Undefined();
}


static void CheckStatus(uv_timer_t* watcher, int status) {
  assert(watcher == &gc_timer);

  // check memory
  if (!uv_is_active((uv_handle_t*) &gc_idle)) {
    HeapStatistics stats;
    V8::GetHeapStatistics(&stats);
    if (stats.total_heap_size() > 1024 * 1024 * 128) {
      // larger than 128 megs, just start the idle watcher
      uv_idle_start(&gc_idle, node::Idle);
      return;
    }
  }

  double d = uv_now(uv_default_loop()) - TICK_TIME(3);

  //printfb("timer d = %f\n", d);

  if (d  >= GC_WAIT_TIME - 1.) {
    //fprintf(stderr, "start idle\n");
    uv_idle_start(&gc_idle, node::Idle);
  }
}

static Handle<Value> Uptime(const Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);

  double uptime =  Platform::GetUptime(true);

  if (uptime < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(uptime));
}


v8::Handle<v8::Value> UVCounters(const v8::Arguments& args) {
  HandleScope scope;

  uv_counters_t* c = &uv_default_loop()->counters;

  Local<Object> obj = Object::New();

#define setc(name) \
    obj->Set(String::New(#name), Integer::New(static_cast<int32_t>(c->name)));

  setc(eio_init)
  setc(req_init)
  setc(handle_init)
  setc(stream_init)
  setc(tcp_init)
  setc(udp_init)
  setc(pipe_init)
  setc(tty_init)
  setc(prepare_init)
  setc(check_init)
  setc(idle_init)
  setc(async_init)
  setc(timer_init)
  setc(process_init)
  setc(fs_event_init)

#undef setc

  return scope.Close(obj);
}


v8::Handle<v8::Value> MemoryUsage(const v8::Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);

  size_t rss;

  int r = Platform::GetMemory(&rss);

  if (r != 0) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
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


typedef void (UV_DYNAMIC* extInit)(Handle<Object> exports);

// DLOpen is node.dlopen(). Used to load 'module.node' dynamically shared
// objects.
Handle<Value> DLOpen(const v8::Arguments& args) {
  HandleScope scope;
  char symbol[1024], *base, *pos;
  uv_lib_t lib;
  node_module_struct compat_mod;
  uv_err_t err;
  int r;

  if (args.Length() < 2) {
    Local<Value> exception = Exception::Error(
        String::New("process.dlopen takes exactly 2 arguments."));
    return ThrowException(exception);
  }

  String::Utf8Value filename(args[0]->ToString()); // Cast
  Local<Object> target = args[1]->ToObject(); // Cast

  err = uv_dlopen(*filename, &lib);
  if (err.code != UV_OK) {
    Local<Value> exception = Exception::Error(
        String::Concat(String::New("Unable to load shared library "),
        args[0]->ToString()));
    return ThrowException(exception);
  }

  String::Utf8Value path(args[0]->ToString());
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
  if (r <= 0 || r >= sizeof symbol) {
    Local<Value> exception =
        Exception::Error(String::New("Out of memory."));
    return ThrowException(exception);
  }

  // Get the init() function from the dynamically shared object.
  node_module_struct *mod;
  err = uv_dlsym(lib, symbol, reinterpret_cast<void**>(&mod));

  if (err.code != UV_OK) {
    /* Start Compatibility hack: Remove once everyone is using NODE_MODULE macro */
    memset(&compat_mod, 0, sizeof compat_mod);

    mod = &compat_mod;
    mod->version = NODE_MODULE_VERSION;

    err = uv_dlsym(lib, "init", reinterpret_cast<void**>(&mod->register_func));
    if (err.code != UV_OK) {
      uv_dlclose(lib);

      const char* message;
      if (err.code == UV_ENOENT)
        message = "Module entry point not found.";
      else
        message = uv_strerror(err);

      return ThrowException(Exception::Error(String::New(message)));
    }
    /* End Compatibility hack */
  }

  if (mod->version != NODE_MODULE_VERSION) {
    Local<Value> exception = Exception::Error(
        String::New("Module version mismatch, refusing to load."));
    return ThrowException(exception);
  }

  // Execute the C++ module
  mod->register_func(target);

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
  exit(1);
}

static int uncaught_exception_counter = 0;

void FatalException(TryCatch &try_catch) {
  HandleScope scope;

  // Check if uncaught_exception_counter indicates a recursion
  if (uncaught_exception_counter > 0) {
    ReportException(try_catch, true);
    exit(1);
  }

  if (listeners_symbol.IsEmpty()) {
    listeners_symbol = NODE_PSYMBOL("listeners");
    uncaught_exception_symbol = NODE_PSYMBOL("uncaughtException");
    emit_symbol = NODE_PSYMBOL("emit");
  }

  Local<Value> listeners_v = process->Get(listeners_symbol);
  assert(listeners_v->IsFunction());

  Local<Function> listeners = Local<Function>::Cast(listeners_v);

  Local<String> uncaught_exception_symbol_l = Local<String>::New(uncaught_exception_symbol);
  Local<Value> argv[1] = { uncaught_exception_symbol_l  };
  Local<Value> ret = listeners->Call(process, 1, argv);

  assert(ret->IsArray());

  Local<Array> listener_array = Local<Array>::Cast(ret);

  uint32_t length = listener_array->Length();
  // Report and exit if process has no "uncaughtException" listener
  if (length == 0) {
    ReportException(try_catch, true);
    exit(1);
  }

  // Otherwise fire the process "uncaughtException" event
  Local<Value> emit_v = process->Get(emit_symbol);
  assert(emit_v->IsFunction());

  Local<Function> emit = Local<Function>::Cast(emit_v);

  Local<Value> error = try_catch.Exception();
  Local<Value> event_argv[2] = { uncaught_exception_symbol_l, error };

  uncaught_exception_counter++;
  emit->Call(process, 2, event_argv);
  // Decrement so we know if the next exception is a recursion or not
  uncaught_exception_counter--;
}


static uv_async_t debug_watcher;

static void DebugMessageCallback(uv_async_t* watcher, int status) {
  HandleScope scope;
  assert(watcher == &debug_watcher);
  Debug::ProcessDebugMessages();
}

static void DebugMessageDispatch(void) {
  // This function is called from V8's debug thread when a debug TCP client
  // has sent a message.

  // Send a signal to our main thread saying that it should enter V8 to
  // handle the message.
  uv_async_send(&debug_watcher);
}

static void DebugBreakMessageHandler(const Debug::Message& message) {
  // do nothing with debug messages.
  // The message handler will get changed by DebuggerAgent::CreateSession in
  // debug-agent.cc of v8/src when a new session is created
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
    modp->register_func(exports);
    binding_cache->Set(module, exports);

  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New();
    DefineConstants(exports);
    binding_cache->Set(module, exports);

#ifdef __POSIX__
  } else if (!strcmp(*module_v, "io_watcher")) {
    exports = Object::New();
    IOWatcher::Initialize(exports);
    binding_cache->Set(module, exports);
#endif

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
  int len;
  const char *s = Platform::GetProcessTitle(&len);
  return scope.Close(s ? String::New(s, len) : String::Empty());
}


static void ProcessTitleSetter(Local<String> property,
                               Local<Value> value,
                               const AccessorInfo& info) {
  HandleScope scope;
  String::Utf8Value title(value->ToString());
  Platform::SetProcessTitle(*title);
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
  // Not found
  return Undefined();
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
    return scope.Close(Integer::New(None));
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
      return scope.Close(Integer::New(None));
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
  // prototyped as `void unsetenv(const char*)` on some platforms
  if (unsetenv(*key) < 0) {
    // Deletion failed. Return true if the key wasn't there in the first place,
    // false if it is still there.
    return scope.Close(Boolean::New(getenv(*key) == NULL));
  };
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
#endif
  // It worked
  return v8::True();
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


static Handle<Value> DebugProcess(const Arguments& args);

Handle<Object> SetupProcessObject(int argc, char *argv[]) {
  HandleScope scope;

  int i, j;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();

  process = Persistent<Object>::New(process_template->GetFunction()->NewInstance());


  process->SetAccessor(String::New("title"),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

  // process.version
  process->Set(String::NewSymbol("version"), String::New(NODE_VERSION));

#ifdef NODE_PREFIX
  // process.installPrefix
  process->Set(String::NewSymbol("installPrefix"), String::New(NODE_PREFIX));
#endif

  // process.moduleLoadList
  module_load_list = Persistent<Array>::New(Array::New());
  process->Set(String::NewSymbol("moduleLoadList"), module_load_list);

  Local<Object> versions = Object::New();
  char buf[20];
  process->Set(String::NewSymbol("versions"), versions);
  // +1 to get rid of the leading 'v'
  versions->Set(String::NewSymbol("node"), String::New(NODE_VERSION+1));
  versions->Set(String::NewSymbol("v8"), String::New(V8::GetVersion()));
  versions->Set(String::NewSymbol("ares"), String::New(ARES_VERSION_STR));
  snprintf(buf, 20, "%d.%d", UV_VERSION_MAJOR, UV_VERSION_MINOR);
  versions->Set(String::NewSymbol("uv"), String::New(buf));
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

  // create process.env
  Local<ObjectTemplate> envTemplate = ObjectTemplate::New();
  envTemplate->SetNamedPropertyHandler(EnvGetter,
                                       EnvSetter,
                                       EnvQuery,
                                       EnvDeleter,
                                       EnvEnumerator,
                                       Undefined());
  Local<Object> env = envTemplate->NewInstance();
  process->Set(String::NewSymbol("env"), env);

  process->Set(String::NewSymbol("pid"), Integer::New(getpid()));
  process->Set(String::NewSymbol("features"), GetFeatures());

  // -e, --eval
  if (eval_string) {
    process->Set(String::NewSymbol("_eval"), String::New(eval_string));
    process->Set(String::NewSymbol("_print_eval"), Boolean::New(print_eval));
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


  // define various internal methods
  NODE_SET_METHOD(process, "_needTickCallback", NeedTickCallback);
  NODE_SET_METHOD(process, "reallyExit", Exit);
  NODE_SET_METHOD(process, "chdir", Chdir);
  NODE_SET_METHOD(process, "cwd", Cwd);

  NODE_SET_METHOD(process, "umask", Umask);

#ifdef __POSIX__
  NODE_SET_METHOD(process, "getuid", GetUid);
  NODE_SET_METHOD(process, "setuid", SetUid);

  NODE_SET_METHOD(process, "setgid", SetGid);
  NODE_SET_METHOD(process, "getgid", GetGid);
#endif // __POSIX__

  NODE_SET_METHOD(process, "_kill", Kill);

  NODE_SET_METHOD(process, "_debugProcess", DebugProcess);

  NODE_SET_METHOD(process, "dlopen", DLOpen);

  NODE_SET_METHOD(process, "uptime", Uptime);
  NODE_SET_METHOD(process, "memoryUsage", MemoryUsage);
  NODE_SET_METHOD(process, "uvCounters", UVCounters);

  NODE_SET_METHOD(process, "binding", Binding);

  return process;
}


static void AtExit() {
  uv_tty_reset_mode();
}


static void SignalExit(int signal) {
  uv_tty_reset_mode();
  _exit(1);
}


void Load(Handle<Object> process_l) {
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

#ifdef HAVE_DTRACE
  InitDTrace(global);
#endif

  f->Call(global, 1, args);

  if (try_catch.HasCaught())  {
    ReportException(try_catch, true);
    exit(11);
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
  exit(1);
}

static void PrintHelp() {
  printf("Usage: node [options] [ -e script | script.js ] [arguments] \n"
         "       node debug script.js [arguments] \n"
         "\n"
         "Options:\n"
         "  -v, --version        print node's version\n"
         "  -e, --eval script    evaluate script\n"
         "  -p, --print          print result of --eval\n"
         "  --v8-options         print v8 command line options\n"
         "  --vars               print various compiled-in variables\n"
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
    } else if (strcmp(arg, "--vars") == 0) {
#ifdef NODE_PREFIX
      printf("NODE_PREFIX: %s\n", NODE_PREFIX);
#endif
#ifdef NODE_CFLAGS
      printf("NODE_CFLAGS: %s\n", NODE_CFLAGS);
#endif
      exit(0);
    } else if (strstr(arg, "--max-stack-size=") == arg) {
      const char *p = 0;
      p = 1 + strchr(arg, '=');
      max_stack_size = atoi(p);
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0 || strcmp(arg, "-e") == 0 ||
        strcmp(arg, "-pe") == 0) {
      if (argc <= i + 1) {
        fprintf(stderr, "Error: --eval requires an argument\n");
        exit(1);
      }
      if (arg[1] == 'p') {
        print_eval = true;
      }
      argv[i] = const_cast<char*>("");
      eval_string = argv[++i];
    } else if (strcmp(arg, "--print") == 0 || strcmp(arg, "-p") == 0) {
      print_eval = true;
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = const_cast<char*>("--help");
    } else if (argv[i][0] != '-') {
      break;
    }
  }

  option_end_index = i;
}


static Isolate* node_isolate = NULL;
static volatile bool debugger_running = false;

static void EnableDebug(bool wait_connect) {
  // If we're called from another thread, make sure to enter the right
  // v8 isolate.
  node_isolate->Enter();

  // Start the debug thread and it's associated TCP server on port 5858.
  bool r = Debug::EnableAgent("node " NODE_VERSION, debug_port);

  if (wait_connect) {
    // Set up an empty handler so v8 will not continue until a debugger
    // attaches. This is the same behavior as Debug::EnableAgent(_,_,true)
    // except we don't break at the beginning of the script.
    // see Debugger::StartAgent in debug.cc of v8/src
    Debug::SetMessageHandler2(node::DebugBreakMessageHandler);
  }

  // Crappy check that everything went well. FIXME
  assert(r);

  // Print out some information.
  fprintf(stderr, "debugger listening on port %d\n", debug_port);
  fflush(stderr);

  debugger_running = true;

  node_isolate->Exit();
}


#ifdef __POSIX__
static void EnableDebugSignalHandler(int signal) {
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


char** Init(int argc, char *argv[]) {
  // Hack aroung with the argv pointer. Used for process.title = "blah".
  argv = node::Platform::SetupArgs(argc, argv);

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
    memcpy(v8argv, argv, sizeof(argv) * option_end_index);
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

  uv_prepare_init(uv_default_loop(), &prepare_tick_watcher);
  uv_prepare_start(&prepare_tick_watcher, PrepareTick);
  uv_unref(uv_default_loop());

  uv_check_init(uv_default_loop(), &check_tick_watcher);
  uv_check_start(&check_tick_watcher, node::CheckTick);
  uv_unref(uv_default_loop());

  uv_idle_init(uv_default_loop(), &tick_spinner);
  uv_unref(uv_default_loop());

  uv_check_init(uv_default_loop(), &gc_check);
  uv_check_start(&gc_check, node::Check);
  uv_unref(uv_default_loop());

  uv_idle_init(uv_default_loop(), &gc_idle);
  uv_unref(uv_default_loop());

  uv_timer_init(uv_default_loop(), &gc_timer);
  uv_unref(uv_default_loop());

  V8::SetFatalErrorHandler(node::OnFatalError);


  // Set the callback DebugMessageDispatch which is called from the debug
  // thread.
  Debug::SetDebugMessageDispatchHandler(node::DebugMessageDispatch);

  // Initialize the async watcher. DebugMessageCallback() is called from the
  // main thread to execute a random bit of javascript - which will give V8
  // control so it can handle whatever new message had been received on the
  // debug thread.
  uv_async_init(uv_default_loop(), &node::debug_watcher,
      node::DebugMessageCallback);
  // unref it so that we exit the event loop despite it being active.
  uv_unref(uv_default_loop());

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
    RegisterSignalHandler(SIGUSR1, EnableDebugSignalHandler);
#endif // __POSIX__
  }

  return argv;
}


void EmitExit(v8::Handle<v8::Object> process_l) {
  // process.emit('exit')
  Local<Value> emit_v = process_l->Get(String::New("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit = Local<Function>::Cast(emit_v);
  Local<Value> args[] = { String::New("exit") };
  TryCatch try_catch;
  emit->Call(process_l, 1, args);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


int Start(int argc, char *argv[]) {
  // This needs to run *before* V8::Initialize()
  argv = Init(argc, argv);

  v8::V8::Initialize();
  v8::HandleScope handle_scope;

  // Create the one and only Context.
  Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);

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
  uv_run(uv_default_loop());

  EmitExit(process_l);

#ifndef NDEBUG
  // Clean up.
  context.Dispose();
  V8::Dispose();
#endif  // NDEBUG

  return 0;
}


}  // namespace node

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
#include <node_dtrace.h>

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h> /* setuid, getuid */

#ifdef __MINGW32__
# include <platform_win32.h> /* winapi_perror() */
# include <platform_win32_winsock.h> /* wsa_init() */
#endif

#ifdef __POSIX__
# include <dlfcn.h> /* dlopen(), dlsym() */
# include <pwd.h> /* getpwnam() */
# include <grp.h> /* getgrnam() */
#endif

#include <platform.h>
#include <node_buffer.h>
#ifdef __POSIX__
# include <node_io_watcher.h>
#endif
#include <node_net.h>
#include <node_events.h>
#include <node_cares.h>
#include <node_file.h>
#include <node_http_parser.h>
#ifdef __POSIX__
# include <node_signal_watcher.h>
# include <node_stat_watcher.h>
#endif
#include <node_child_process.h>
#include <node_constants.h>
#include <node_stdio.h>
#include <node_javascript.h>
#include <node_version.h>
#include <node_string.h>
#ifdef HAVE_OPENSSL
# include <node_crypto.h>
#endif
#include <node_script.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

using namespace v8;

# ifdef __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
# else
extern char **environ;
# endif

namespace node {

static Persistent<Object> process;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> errpath_symbol;
static Persistent<String> code_symbol;

static Persistent<String> rss_symbol;
static Persistent<String> vsize_symbol;
static Persistent<String> heap_total_symbol;
static Persistent<String> heap_used_symbol;

static Persistent<String> listeners_symbol;
static Persistent<String> uncaught_exception_symbol;
static Persistent<String> emit_symbol;


static char *eval_string = NULL;
static int option_end_index = 0;
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static bool cov = false;
static int debug_port=5858;
static int max_stack_size = 0;

static uv_check_t check_tick_watcher;
static uv_prepare_t prepare_tick_watcher;
static uv_idle_t tick_spinner;
static bool need_tick_cb;
static Persistent<String> tick_callback_sym;

static uv_async_t eio_want_poll_notifier;
static uv_async_t eio_done_poll_notifier;
static uv_idle_t eio_poller;

// Buffer for getpwnam_r(), getgrpam_r() and other misc callers; keep this
// scoped at file-level rather than method-level to avoid excess stack usage.
static char getbuf[PATH_MAX + 1];

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


#define FAST_TICK 0.7
#define GC_WAIT_TIME 5.
#define RPM_SAMPLES 100
#define TICK_TIME(n) tick_times[(tick_time_head - (n)) % RPM_SAMPLES]
static int64_t tick_times[RPM_SAMPLES];
static int tick_time_head;

static void CheckStatus(uv_handle_t* watcher, int status);

static void StartGCTimer () {
  if (!uv_is_active((uv_handle_t*) &gc_timer)) {
    uv_timer_start(&node::gc_timer, node::CheckStatus, 5., 5.);
  }
}

static void StopGCTimer () {
  if (uv_is_active((uv_handle_t*) &gc_timer)) {
    uv_timer_stop(&gc_timer);
  }
}

static void Idle(uv_handle_t* watcher, int status) {
  assert((uv_idle_t*) watcher == &gc_idle);

  //fprintf(stderr, "idle\n");

  if (V8::IdleNotification()) {
    uv_idle_stop(&gc_idle);
    StopGCTimer();
  }
}


// Called directly after every call to select() (or epoll, or whatever)
static void Check(uv_handle_t* watcher, int status) {
  assert((uv_check_t*) watcher == &gc_check);

  tick_times[tick_time_head] = uv_now();
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
  uv_idle_start(&node::gc_idle, node::Idle);
}


static void Tick(void) {
  // Avoid entering a V8 scope.
  if (!need_tick_cb) return;

  need_tick_cb = false;
  if (uv_is_active((uv_handle_t*) &tick_spinner)) {
    uv_idle_stop(&tick_spinner);
    uv_unref();
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


static void Spin(uv_handle_t* handle, int status) {
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
    uv_ref();
  }
  return Undefined();
}


static void PrepareTick(uv_handle_t* handle, int status) {
  assert((uv_prepare_t*) handle == &prepare_tick_watcher);
  assert(status == 0);
  Tick();
}


static void CheckTick(uv_handle_t* handle, int status) {
  assert((uv_check_t*) handle == &check_tick_watcher);
  assert(status == 0);
  Tick();
}


static void DoPoll(uv_handle_t* watcher, int status) {
  assert((uv_idle_t*) watcher == &eio_poller);

  //printf("eio_poller\n");

  if (eio_poll() != -1 && uv_is_active((uv_handle_t*) &eio_poller)) {
    //printf("eio_poller stop\n");
    uv_idle_stop(&eio_poller);
    uv_unref();
  }
}


// Called from the main thread.
static void WantPollNotifier(uv_handle_t* watcher, int status) {
  assert((uv_async_t*) watcher == &eio_want_poll_notifier);

  //printf("want poll notifier\n");

  if (eio_poll() == -1 && !uv_is_active((uv_handle_t*) &eio_poller)) {
    //printf("eio_poller start\n");
    uv_idle_start(&eio_poller, node::DoPoll);
    uv_ref();
  }
}


static void DonePollNotifier(uv_handle_t* watcher, int revents) {
  assert((uv_async_t*) watcher == &eio_done_poll_notifier);

  //printf("done poll notifier\n");

  if (eio_poll() != -1 && uv_is_active((uv_handle_t*) &eio_poller)) {
    //printf("eio_poller stop\n");
    uv_idle_stop(&eio_poller);
    uv_unref();
  }
}


// EIOWantPoll() is called from the EIO thread pool each time an EIO
// request (that is, one of the node.fs.* functions) has completed.
static void EIOWantPoll(void) {
  // Signal the main thread that eio_poll need to be processed.
  uv_async_send(&eio_want_poll_notifier);
}


static void EIODonePoll(void) {
  // Signal the main thread that we should stop calling eio_poll().
  // from the idle watcher.
  uv_async_send(&eio_done_poll_notifier);
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

#ifdef WSAEINTR
  ERRNO_CASE(WSAEINTR);
#endif

#ifdef WSAEBADF
  ERRNO_CASE(WSAEBADF);
#endif

#ifdef WSAEACCES
  ERRNO_CASE(WSAEACCES);
#endif

#ifdef WSAEFAULT
  ERRNO_CASE(WSAEFAULT);
#endif

#ifdef WSAEINVAL
  ERRNO_CASE(WSAEINVAL);
#endif

#ifdef WSAEMFILE
  ERRNO_CASE(WSAEMFILE);
#endif

#ifdef WSAEWOULDBLOCK
  ERRNO_CASE(WSAEWOULDBLOCK);
#endif

#ifdef WSAEINPROGRESS
  ERRNO_CASE(WSAEINPROGRESS);
#endif

#ifdef WSAEALREADY
  ERRNO_CASE(WSAEALREADY);
#endif

#ifdef WSAENOTSOCK
  ERRNO_CASE(WSAENOTSOCK);
#endif

#ifdef WSAEDESTADDRREQ
  ERRNO_CASE(WSAEDESTADDRREQ);
#endif

#ifdef WSAEMSGSIZE
  ERRNO_CASE(WSAEMSGSIZE);
#endif

#ifdef WSAEPROTOTYPE
  ERRNO_CASE(WSAEPROTOTYPE);
#endif

#ifdef WSAENOPROTOOPT
  ERRNO_CASE(WSAENOPROTOOPT);
#endif

#ifdef WSAEPROTONOSUPPORT
  ERRNO_CASE(WSAEPROTONOSUPPORT);
#endif

#ifdef WSAESOCKTNOSUPPORT
  ERRNO_CASE(WSAESOCKTNOSUPPORT);
#endif

#ifdef WSAEOPNOTSUPP
  ERRNO_CASE(WSAEOPNOTSUPP);
#endif

#ifdef WSAEPFNOSUPPORT
  ERRNO_CASE(WSAEPFNOSUPPORT);
#endif

#ifdef WSAEAFNOSUPPORT
  ERRNO_CASE(WSAEAFNOSUPPORT);
#endif

#ifdef WSAEADDRINUSE
  ERRNO_CASE(WSAEADDRINUSE);
#endif

#ifdef WSAEADDRNOTAVAIL
  ERRNO_CASE(WSAEADDRNOTAVAIL);
#endif

#ifdef WSAENETDOWN
  ERRNO_CASE(WSAENETDOWN);
#endif

#ifdef WSAENETUNREACH
  ERRNO_CASE(WSAENETUNREACH);
#endif

#ifdef WSAENETRESET
  ERRNO_CASE(WSAENETRESET);
#endif

#ifdef WSAECONNABORTED
  ERRNO_CASE(WSAECONNABORTED);
#endif

#ifdef WSAECONNRESET
  ERRNO_CASE(WSAECONNRESET);
#endif

#ifdef WSAENOBUFS
  ERRNO_CASE(WSAENOBUFS);
#endif

#ifdef WSAEISCONN
  ERRNO_CASE(WSAEISCONN);
#endif

#ifdef WSAENOTCONN
  ERRNO_CASE(WSAENOTCONN);
#endif

#ifdef WSAESHUTDOWN
  ERRNO_CASE(WSAESHUTDOWN);
#endif

#ifdef WSAETOOMANYREFS
  ERRNO_CASE(WSAETOOMANYREFS);
#endif

#ifdef WSAETIMEDOUT
  ERRNO_CASE(WSAETIMEDOUT);
#endif

#ifdef WSAECONNREFUSED
  ERRNO_CASE(WSAECONNREFUSED);
#endif

#ifdef WSAELOOP
  ERRNO_CASE(WSAELOOP);
#endif

#ifdef WSAENAMETOOLONG
  ERRNO_CASE(WSAENAMETOOLONG);
#endif

#ifdef WSAEHOSTDOWN
  ERRNO_CASE(WSAEHOSTDOWN);
#endif

#ifdef WSAEHOSTUNREACH
  ERRNO_CASE(WSAEHOSTUNREACH);
#endif

#ifdef WSAENOTEMPTY
  ERRNO_CASE(WSAENOTEMPTY);
#endif

#ifdef WSAEPROCLIM
  ERRNO_CASE(WSAEPROCLIM);
#endif

#ifdef WSAEUSERS
  ERRNO_CASE(WSAEUSERS);
#endif

#ifdef WSAEDQUOT
  ERRNO_CASE(WSAEDQUOT);
#endif

#ifdef WSAESTALE
  ERRNO_CASE(WSAESTALE);
#endif

#ifdef WSAEREMOTE
  ERRNO_CASE(WSAEREMOTE);
#endif

#ifdef WSASYSNOTREADY
  ERRNO_CASE(WSASYSNOTREADY);
#endif

#ifdef WSAVERNOTSUPPORTED
  ERRNO_CASE(WSAVERNOTSUPPORTED);
#endif

#ifdef WSANOTINITIALISED
  ERRNO_CASE(WSANOTINITIALISED);
#endif

#ifdef WSAEDISCON
  ERRNO_CASE(WSAEDISCON);
#endif

#ifdef WSAENOMORE
  ERRNO_CASE(WSAENOMORE);
#endif

#ifdef WSAECANCELLED
  ERRNO_CASE(WSAECANCELLED);
#endif

#ifdef WSAEINVALIDPROCTABLE
  ERRNO_CASE(WSAEINVALIDPROCTABLE);
#endif

#ifdef WSAEINVALIDPROVIDER
  ERRNO_CASE(WSAEINVALIDPROVIDER);
#endif

#ifdef WSAEPROVIDERFAILEDINIT
  ERRNO_CASE(WSAEPROVIDERFAILEDINIT);
#endif

#ifdef WSASYSCALLFAILURE
  ERRNO_CASE(WSASYSCALLFAILURE);
#endif

#ifdef WSASERVICE_NOT_FOUND
  ERRNO_CASE(WSASERVICE_NOT_FOUND);
#endif

#ifdef WSATYPE_NOT_FOUND
  ERRNO_CASE(WSATYPE_NOT_FOUND);
#endif

#ifdef WSA_E_NO_MORE
  ERRNO_CASE(WSA_E_NO_MORE);
#endif

#ifdef WSA_E_CANCELLED
  ERRNO_CASE(WSA_E_CANCELLED);
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
#ifdef __POSIX__
    msg = strerror(errorno);
#else // __MINGW32__
    msg = winapi_strerror(errorno);
#endif
  }
  Local<String> message = String::NewSymbol(msg);

  Local<String> cons1 = String::Concat(estring, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  if (errno_symbol.IsEmpty()) {
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
  assert(callback_v->IsFunction());
  Local<Function> callback = Local<Function>::Cast(callback_v);

  // TODO Hook for long stack traces to be made here.

  TryCatch try_catch;

  callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


void SetErrno(uv_err_code code) {
  uv_err_t err;
  err.code = code;
  Context::GetCurrent()->Global()->Set(String::NewSymbol("errno"),
                                       String::NewSymbol(uv_err_name(err)));
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

  node::Stdio::DisableRawMode(STDIN_FILENO);
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

  int r = chdir(*path);

  if (r != 0) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }

  return Undefined();
}

static Handle<Value> Cwd(const Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);

  char *r = getcwd(getbuf, ARRAY_SIZE(getbuf) - 1);
  if (r == NULL) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }

  getbuf[ARRAY_SIZE(getbuf) - 1] = '\0';
  Local<String> cwd = String::New(r);

  return scope.Close(cwd);
}


#ifdef __POSIX__

static Handle<Value> Umask(const Arguments& args){
  HandleScope scope;
  unsigned int old;

  if(args.Length() < 1 || args[0]->IsUndefined()) {
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


static void CheckStatus(uv_handle_t* watcher, int status) {
  assert((uv_timer_t*) watcher == &gc_timer);

  // check memory
  if (!uv_is_active((uv_handle_t*) &gc_idle)) {
    HeapStatistics stats;
    V8::GetHeapStatistics(&stats);
    if (stats.total_heap_size() > 1024 * 1024 * 128) {
      // larger than 128 megs, just start the idle watcher
      uv_idle_start(&node::gc_idle, node::Idle);
      return;
    }
  }

  double d = uv_now() - TICK_TIME(3);

  //printfb("timer d = %f\n", d);

  if (d  >= GC_WAIT_TIME - 1.) {
    //fprintf(stderr, "start idle\n");
    uv_idle_start(&node::gc_idle, node::Idle);
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

v8::Handle<v8::Value> MemoryUsage(const v8::Arguments& args) {
  HandleScope scope;
  assert(args.Length() == 0);

  size_t rss, vsize;

  int r = Platform::GetMemory(&rss, &vsize);

  if (r != 0) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }

  Local<Object> info = Object::New();

  if (rss_symbol.IsEmpty()) {
    rss_symbol = NODE_PSYMBOL("rss");
    vsize_symbol = NODE_PSYMBOL("vsize");
    heap_total_symbol = NODE_PSYMBOL("heapTotal");
    heap_used_symbol = NODE_PSYMBOL("heapUsed");
  }

  info->Set(rss_symbol, Integer::NewFromUnsigned(rss));
  info->Set(vsize_symbol, Integer::NewFromUnsigned(vsize));

  // V8 memory usage
  HeapStatistics v8_heap_stats;
  V8::GetHeapStatistics(&v8_heap_stats);
  info->Set(heap_total_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.total_heap_size()));
  info->Set(heap_used_symbol,
            Integer::NewFromUnsigned(v8_heap_stats.used_heap_size()));

  return scope.Close(info);
}


#ifdef __POSIX__

Handle<Value> Kill(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 2) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }

  pid_t pid = args[0]->IntegerValue();
  int sig = args[1]->Int32Value();
  int r = kill(pid, sig);

  if (r != 0) return ThrowException(ErrnoException(errno, "kill"));

  return Undefined();
}


typedef void (*extInit)(Handle<Object> exports);

// DLOpen is node.dlopen(). Used to load 'module.node' dynamically shared
// objects.
Handle<Value> DLOpen(const v8::Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) return Undefined();

  String::Utf8Value filename(args[0]->ToString()); // Cast
  Local<Object> target = args[1]->ToObject(); // Cast

  // Actually call dlopen().
  // FIXME: This is a blocking function and should be called asynchronously!
  // This function should be moved to file.cc and use libeio to make this
  // system call.
  void *handle = dlopen(*filename, RTLD_LAZY);

  // Handle errors.
  if (handle == NULL) {
    Local<Value> exception = Exception::Error(String::New(dlerror()));
    return ThrowException(exception);
  }

  String::Utf8Value symbol(args[0]->ToString());
  char *symstr = NULL;
  {
    char *sym = *symbol;
    char *p = strrchr(sym, '/');
    if (p != NULL) {
      sym = p+1;
    }

    p = strrchr(sym, '.');
    if (p != NULL) {
      *p = '\0';
    }

    size_t slen = strlen(sym);
    symstr = static_cast<char*>(calloc(1, slen + sizeof("_module") + 1));
    memcpy(symstr, sym, slen);
    memcpy(symstr+slen, "_module", sizeof("_module") + 1);
  }

  // Get the init() function from the dynamically shared object.
  node_module_struct *mod = static_cast<node_module_struct *>(dlsym(handle, symstr));
  free(symstr);
  // Error out if not found.
  if (mod == NULL) {
    /* Start Compatibility hack: Remove once everyone is using NODE_MODULE macro */
    node_module_struct compat_mod;
    mod = &compat_mod;
    mod->version = NODE_MODULE_VERSION;

    void *init_handle = dlsym(handle, "init");
    if (init_handle == NULL) {
      dlclose(handle);
      Local<Value> exception =
        Exception::Error(String::New("No module symbol found in module."));
      return ThrowException(exception);
    }
    mod->register_func = (extInit)(init_handle);
    /* End Compatibility hack */
  }

  if (mod->version != NODE_MODULE_VERSION) {
    Local<Value> exception =
      Exception::Error(String::New("Module version mismatch, refusing to load."));
    return ThrowException(exception);
  }

  // Execute the C++ module
  mod->register_func(target);

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
  return Undefined();
}

#endif // __POSIX__


// TODO remove me before 0.4
Handle<Value> Compile(const Arguments& args) {
  HandleScope scope;


  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("needs two arguments.")));
  }

  static bool shown_error_message = false;

  if (!shown_error_message) {
    shown_error_message = true;
    fprintf(stderr, "(node) process.compile should not be used. "
                    "Use require('vm').runInThisContext instead.\n");
  }

  Local<String> source = args[0]->ToString();
  Local<String> filename = args[1]->ToString();

  TryCatch try_catch;

  Local<v8::Script> script = v8::Script::Compile(source, filename);
  if (try_catch.HasCaught()) {
    // Hack because I can't get a proper stacktrace on SyntaxError
    ReportException(try_catch, true);
    exit(1);
  }

  Local<Value> result = script->Run();
  if (try_catch.HasCaught()) {
    ReportException(try_catch, false);
    exit(1);
  }

  return scope.Close(result);
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

static void DebugMessageCallback(uv_handle_t* watcher, int status) {
  HandleScope scope;
  assert((uv_async_t*) watcher == &debug_watcher);
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

  } else if ((modp = get_builtin_module(*module_v)) != NULL) {
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
  String::Utf8Value key(property);
  const char* val = getenv(*key);
  if (val) {
    HandleScope scope;
    return scope.Close(String::New(val));
  }
  return Undefined();
}


static bool ENV_warning = false;
static Handle<Value> EnvGetterWarn(Local<String> property,
                                   const AccessorInfo& info) {
  if (!ENV_warning) {
    ENV_warning = true;
    fprintf(stderr, "(node) Use process.env instead of process.ENV\r\n");
  }
  return EnvGetter(property, info);
}


static Handle<Value> EnvSetter(Local<String> property,
                               Local<Value> value,
                               const AccessorInfo& info) {
  String::Utf8Value key(property);
  String::Utf8Value val(value);
#ifdef __POSIX__
  setenv(*key, *val, 1);
#else  // __WIN32__
  NO_IMPL_MSG(setenv)
#endif
  return value;
}


static Handle<Integer> EnvQuery(Local<String> property,
                                const AccessorInfo& info) {
  String::Utf8Value key(property);
  if (getenv(*key)) {
    HandleScope scope;
    return scope.Close(Integer::New(None));
  }
  return Handle<Integer>();
}


static Handle<Boolean> EnvDeleter(Local<String> property,
                                  const AccessorInfo& info) {
  String::Utf8Value key(property);
  if (getenv(*key)) {
#ifdef __POSIX__
    unsetenv(*key);	// prototyped as `void unsetenv(const char*)` on some platforms
#else
    NO_IMPL_MSG(unsetenv)
#endif
    return True();
  }
  return False();
}


static Handle<Array> EnvEnumerator(const AccessorInfo& info) {
  HandleScope scope;

  int size = 0;
  while (environ[size]) size++;

  Local<Array> env = Array::New(size);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    env->Set(i, String::New(var, length));
  }

  return scope.Close(env);
}


Handle<Object> SetupProcessObject(int argc, char *argv[]) {
  HandleScope scope;

  int i, j;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();
  node::EventEmitter::Initialize(process_template);

  process = Persistent<Object>::New(process_template->GetFunction()->NewInstance());


  process->SetAccessor(String::New("title"),
                       ProcessTitleGetter,
                       ProcessTitleSetter);

  // process.version
  process->Set(String::NewSymbol("version"), String::New(NODE_VERSION));

  // process.installPrefix
  process->Set(String::NewSymbol("installPrefix"), String::New(NODE_PREFIX));

  Local<Object> versions = Object::New();
  char buf[20];
  process->Set(String::NewSymbol("versions"), versions);
  // +1 to get rid of the leading 'v'
  versions->Set(String::NewSymbol("node"), String::New(NODE_VERSION+1));
  versions->Set(String::NewSymbol("v8"), String::New(V8::GetVersion()));
  versions->Set(String::NewSymbol("ares"), String::New(ARES_VERSION_STR));
  snprintf(buf, 20, "%d.%d", UV_VERSION_MAJOR, UV_VERSION_MINOR);
  versions->Set(String::NewSymbol("uv"), String::New(buf));
#ifdef HAVE_OPENSSL
  // Stupid code to slice out the version string.
  int c, l = strlen(OPENSSL_VERSION_TEXT);
  for (i = 0; i < l; i++) {
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
  process->Set(String::NewSymbol("ARGV"), arguments);
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

  // create process.ENV
  // TODO: remove me at some point.
  Local<ObjectTemplate> ENVTemplate = ObjectTemplate::New();
  ENVTemplate->SetNamedPropertyHandler(EnvGetterWarn,
                                       EnvSetter,
                                       EnvQuery,
                                       EnvDeleter,
                                       EnvEnumerator,
                                       Undefined());
  Local<Object> ENV = ENVTemplate->NewInstance();
  process->Set(String::NewSymbol("ENV"), ENV);

  process->Set(String::NewSymbol("pid"), Integer::New(getpid()));
  process->Set(String::NewSymbol("cov"), cov ? True() : False());

  // -e, --eval
  if (eval_string) {
    process->Set(String::NewSymbol("_eval"), String::New(eval_string));
  }

  size_t size = 2*PATH_MAX;
  char execPath[size];
  if (Platform::GetExecutablePath(execPath, &size) != 0) {
    // as a last ditch effort, fallback on argv[0] ?
    process->Set(String::NewSymbol("execPath"), String::New(argv[0]));
  } else {
    process->Set(String::NewSymbol("execPath"), String::New(execPath, size));
  }


  // define various internal methods
  NODE_SET_METHOD(process, "compile", Compile);
  NODE_SET_METHOD(process, "_needTickCallback", NeedTickCallback);
  NODE_SET_METHOD(process, "reallyExit", Exit);
  NODE_SET_METHOD(process, "chdir", Chdir);
  NODE_SET_METHOD(process, "cwd", Cwd);

#ifdef __POSIX__
  NODE_SET_METHOD(process, "getuid", GetUid);
  NODE_SET_METHOD(process, "setuid", SetUid);

  NODE_SET_METHOD(process, "setgid", SetGid);
  NODE_SET_METHOD(process, "getgid", GetGid);

  NODE_SET_METHOD(process, "umask", Umask);
  NODE_SET_METHOD(process, "dlopen", DLOpen);
  NODE_SET_METHOD(process, "_kill", Kill);
#endif // __POSIX__

  NODE_SET_METHOD(process, "uptime", Uptime);
  NODE_SET_METHOD(process, "memoryUsage", MemoryUsage);

  NODE_SET_METHOD(process, "binding", Binding);

  // Assign the EventEmitter. It was created in main().
  process->Set(String::NewSymbol("EventEmitter"),
               EventEmitter::constructor_template->GetFunction());

  return process;
}


static void AtExit() {
  node::Stdio::Flush();
  node::Stdio::DisableRawMode(STDIN_FILENO);
}


static void SignalExit(int signal) {
  Stdio::DisableRawMode(STDIN_FILENO);
  _exit(1);
}


void Load(Handle<Object> process) {
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
  Local<Value> args[1] = { Local<Value>::New(process) };

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
  printf("Usage: node [options] script.js [arguments] \n"
         "       node debug script.js [arguments] \n"
         "\n"
         "Options:\n"
         "  -v, --version        print node's version\n"
         "  --v8-options         print v8 command line options\n"
         "  --vars               print various compiled-in variables\n"
         "  --max-stack-size=val set max v8 stack size (bytes)\n"
         "  --cov                code coverage; writes node-cov.json \n"
         "\n"
         "Enviromental variables:\n"
         "NODE_PATH              ':'-separated list of directories\n"
         "                       prefixed to the module search path,\n"
         "                       require.paths.\n"
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
    } else if (!strcmp(arg, "--cov")) {
      cov = true;
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--vars") == 0) {
      printf("NODE_PREFIX: %s\n", NODE_PREFIX);
      printf("NODE_CFLAGS: %s\n", NODE_CFLAGS);
      exit(0);
    } else if (strstr(arg, "--max-stack-size=") == arg) {
      const char *p = 0;
      p = 1 + strchr(arg, '=');
      max_stack_size = atoi(p);
      argv[i] = const_cast<char*>("");
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0 || strcmp(arg, "-e") == 0) {
      if (argc <= i + 1) {
        fprintf(stderr, "Error: --eval requires an argument\n");
        exit(1);
      }
      argv[i] = const_cast<char*>("");
      eval_string = argv[++i];
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = const_cast<char*>("--help");
    } else if (argv[i][0] != '-') {
      break;
    }
  }

  option_end_index = i;
}


static void EnableDebug(bool wait_connect) {
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
  fprintf(stderr, "debugger listening on port %d", debug_port);
}


static volatile bool hit_signal;


static void EnableDebugSignalHandler(int signal) {
  // This is signal safe.
  hit_signal = true;
  v8::Debug::DebugBreak();
}


static void DebugSignalCB(const Debug::EventDetails& details) {
  if (hit_signal && details.GetEvent() == v8::Break) {
    hit_signal = false;
    fprintf(stderr, "Hit SIGUSR1 - starting debugger agent.\n");
    EnableDebug(false);
  }
}


#ifdef __POSIX__

static int RegisterSignalHandler(int signal, void (*handler)(int)) {
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  return sigaction(signal, &sa, NULL);
}
#endif // __POSIX__


char** Init(int argc, char *argv[]) {
  // Hack aroung with the argv pointer. Used for process.title = "blah".
  argv = node::Platform::SetupArgs(argc, argv);

  // Parse a few arguments which are specific to Node.
  node::ParseArgs(argc, argv);
  // Parse the rest of the args (up to the 'option_end_index' (where '--' was
  // in the command line))
  int v8argc = node::option_end_index;
  char **v8argv = argv;

  if (node::debug_wait_connect) {
    // v8argv is a copy of argv up to the script file argument +2 if --debug-brk
    // to expose the v8 debugger js object so that node.js can set
    // a breakpoint on the first line of the startup script
    v8argc += 2;
    v8argv = new char*[v8argc];
    memcpy(v8argv, argv, sizeof(argv) * node::option_end_index);
    v8argv[node::option_end_index] = const_cast<char*>("--expose_debug_as");
    v8argv[node::option_end_index + 1] = const_cast<char*>("v8debug");
  }

  // For the normal stack which moves from high to low addresses when frames
  // are pushed, we can compute the limit as stack_size bytes below the
  // the address of a stack variable (e.g. &stack_var) as an approximation
  // of the start of the stack (we're assuming that we haven't pushed a lot
  // of frames yet).
  if (node::max_stack_size != 0) {
    uint32_t stack_var;
    ResourceConstraints constraints;

    uint32_t *stack_limit = &stack_var - (node::max_stack_size / sizeof(uint32_t));
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

#ifdef __MINGW32__
  // Initialize winsock and soem related caches
  wsa_init();
#endif // __MINGW32__

  uv_prepare_init(&node::prepare_tick_watcher, NULL, NULL);
  uv_prepare_start(&node::prepare_tick_watcher, PrepareTick);
  uv_unref();

  uv_check_init(&node::check_tick_watcher, NULL, NULL);
  uv_check_start(&node::check_tick_watcher, node::CheckTick);
  uv_unref();

  uv_idle_init(&node::tick_spinner, NULL, NULL);
  uv_unref();

  uv_check_init(&node::gc_check, NULL, NULL);
  uv_check_start(&node::gc_check, node::Check);
  uv_unref();

  uv_idle_init(&node::gc_idle, NULL, NULL);
  uv_unref();

  uv_timer_init(&node::gc_timer, NULL, NULL);
  uv_unref();

  // Setup the EIO thread pool. It requires 3, yes 3, watchers.
  {
    uv_idle_init(&node::eio_poller, NULL, NULL);
    uv_idle_start(&eio_poller, node::DoPoll);

    uv_async_init(&node::eio_want_poll_notifier, node::WantPollNotifier, NULL, NULL);
    uv_unref();

    uv_async_init(&node::eio_done_poll_notifier, node::DonePollNotifier, NULL, NULL);
    uv_unref();

    eio_init(node::EIOWantPoll, node::EIODonePoll);
    // Don't handle more than 10 reqs on each eio_poll(). This is to avoid
    // race conditions. See test/simple/test-eio-race.js
    eio_set_max_poll_reqs(10);
  }

  V8::SetFatalErrorHandler(node::OnFatalError);


  // Set the callback DebugMessageDispatch which is called from the debug
  // thread.
  Debug::SetDebugMessageDispatchHandler(node::DebugMessageDispatch);
  // Initialize the async watcher. DebugMessageCallback() is called from the
  // main thread to execute a random bit of javascript - which will give V8
  // control so it can handle whatever new message had been received on the
  // debug thread.
  uv_async_init(&node::debug_watcher, node::DebugMessageCallback, NULL, NULL);
  // unref it so that we exit the event loop despite it being active.
  uv_unref();


  // If the --debug flag was specified then initialize the debug thread.
  if (node::use_debug_agent) {
    EnableDebug(debug_wait_connect);
  } else {
#ifdef __POSIX__
    RegisterSignalHandler(SIGUSR1, EnableDebugSignalHandler);
    Debug::SetDebugEventListener2(DebugSignalCB);
#endif // __POSIX__
  }

  return argv;
}


void EmitExit(v8::Handle<v8::Object> process) {
  // process.emit('exit')
  Local<Value> emit_v = process->Get(String::New("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit = Local<Function>::Cast(emit_v);
  Local<Value> args[] = { String::New("exit") };
  TryCatch try_catch;
  emit->Call(process, 1, args);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


int Start(int argc, char *argv[]) {
  uv_init();
  v8::V8::Initialize();
  v8::HandleScope handle_scope;

  argv = Init(argc, argv);

  // Create the one and only Context.
  Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);

  Handle<Object> process = SetupProcessObject(argc, argv);

  // Create all the objects, load modules, do everything.
  // so your next reading stop should be node::Load()!
  Load(process);

  // All our arguments are loaded. We've evaluated all of the scripts. We
  // might even have created TCP servers. Now we enter the main eventloop. If
  // there are no watchers on the loop (except for the ones that were
  // uv_unref'd) then this function exits. As long as there are active
  // watchers, it blocks.
  uv_run();

  EmitExit(process);

#ifndef NDEBUG
  // Clean up.
  context.Dispose();
  V8::Dispose();
#endif  // NDEBUG
  return 0;
}


}  // namespace node

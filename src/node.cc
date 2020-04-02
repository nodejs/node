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

// ========== local headers ==========

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_binding.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_main_instance.h"
#include "node_metadata.h"
#include "node_native_module_env.h"
#include "node_options-inl.h"
#include "node_perf.h"
#include "node_process.h"
#include "node_report.h"
#include "node_revert.h"
#include "node_v8_platform-inl.h"
#include "node_version.h"

#if HAVE_OPENSSL
#include "node_crypto.h"
#endif

#if defined(NODE_HAVE_I18N_SUPPORT)
#include "node_i18n.h"
#endif

#if HAVE_INSPECTOR
#include "inspector_agent.h"
#include "inspector_io.h"
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
#include "node_dtrace.h"
#endif

#if NODE_USE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif  // NODE_USE_V8_PLATFORM
#include "v8-profiler.h"

#if HAVE_INSPECTOR
#include "inspector/worker_inspector.h"  // ParentInspectorHandle
#endif

#include "large_pages/node_large_page.h"

// ========== global C headers ==========

#include <fcntl.h>  // _O_RDWR
#include <sys/types.h>

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/uvernum.h>
#include <unicode/utypes.h>
#endif


#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#define STDIN_FILENO 0
#else
#include <pthread.h>
#include <sys/resource.h>  // getrlimit, setrlimit
#include <termios.h>       // tcgetattr, tcsetattr
#include <unistd.h>        // STDIN_FILENO, STDERR_FILENO
#endif

// ========== global C++ headers ==========

#include <cerrno>
#include <climits>  // PATH_MAX
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <vector>

namespace node {

using native_module::NativeModuleEnv;

using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::V8;
using v8::Value;

namespace per_process {

// node_revert.h
// Bit flag used to track security reverts.
unsigned int reverted_cve = 0;

// util.h
// Tells whether the per-process V8::Initialize() is called and
// if it is safe to call v8::Isolate::GetCurrent().
bool v8_initialized = false;

// node_internals.h
// process-relative uptime base in nanoseconds, initialized in node::Start()
uint64_t node_start_time;
// Tells whether --prof is passed.
bool v8_is_profiling = false;

// node_v8_platform-inl.h
struct V8Platform v8_platform;
}  // namespace per_process

void SignalExit(int signo) {
  ResetStdio();
#ifdef __FreeBSD__
  // FreeBSD has a nasty bug, see RegisterSignalHandler for details
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(signo, &sa, nullptr), 0);
#endif
  raise(signo);
}

MaybeLocal<Value> ExecuteBootstrapper(Environment* env,
                                      const char* id,
                                      std::vector<Local<String>>* parameters,
                                      std::vector<Local<Value>>* arguments) {
  EscapableHandleScope scope(env->isolate());
  MaybeLocal<Function> maybe_fn =
      NativeModuleEnv::LookupAndCompile(env->context(), id, parameters, env);

  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    return MaybeLocal<Value>();
  }

  MaybeLocal<Value> result = fn->Call(env->context(),
                                      Undefined(env->isolate()),
                                      arguments->size(),
                                      arguments->data());

  // If there was an error during bootstrap, it must be unrecoverable
  // (e.g. max call stack exceeded). Clear the stack so that the
  // AsyncCallbackScope destructor doesn't fail on the id check.
  // There are only two ways to have a stack size > 1: 1) the user manually
  // called MakeCallback or 2) user awaited during bootstrap, which triggered
  // _tickCallback().
  if (result.IsEmpty()) {
    env->async_hooks()->clear_async_id_stack();
  }

  return scope.EscapeMaybe(result);
}

#if HAVE_INSPECTOR
int Environment::InitializeInspector(
    std::unique_ptr<inspector::ParentInspectorHandle> parent_handle) {
  std::string inspector_path;
  bool is_main = !parent_handle;
  if (parent_handle) {
    inspector_path = parent_handle->url();
    inspector_agent_->SetParentHandle(std::move(parent_handle));
  } else {
    inspector_path = argv_.size() > 1 ? argv_[1].c_str() : "";
  }

  CHECK(!inspector_agent_->IsListening());
  // Inspector agent can't fail to start, but if it was configured to listen
  // right away on the websocket port and fails to bind/etc, this will return
  // false.
  inspector_agent_->Start(inspector_path,
                          options_->debug_options(),
                          inspector_host_port(),
                          is_main);
  if (options_->debug_options().inspector_enabled &&
      !inspector_agent_->IsListening()) {
    return 12;  // Signal internal error
  }

  profiler::StartProfilers(this);

  if (inspector_agent_->options().break_node_first_line) {
    inspector_agent_->PauseOnNextJavascriptStatement("Break at bootstrap");
  }

  return 0;
}
#endif  // HAVE_INSPECTOR && NODE_USE_V8_PLATFORM

void Environment::InitializeDiagnostics() {
  isolate_->GetHeapProfiler()->AddBuildEmbedderGraphCallback(
      Environment::BuildEmbedderGraph, this);
  if (options_->trace_uncaught)
    isolate_->SetCaptureStackTraceForUncaughtExceptions(true);

#if defined HAVE_DTRACE || defined HAVE_ETW
  InitDTrace(this);
#endif
}

MaybeLocal<Value> Environment::BootstrapInternalLoaders() {
  EscapableHandleScope scope(isolate_);

  // Create binding loaders
  std::vector<Local<String>> loaders_params = {
      process_string(),
      FIXED_ONE_BYTE_STRING(isolate_, "getLinkedBinding"),
      FIXED_ONE_BYTE_STRING(isolate_, "getInternalBinding"),
      primordials_string()};
  std::vector<Local<Value>> loaders_args = {
      process_object(),
      NewFunctionTemplate(binding::GetLinkedBinding)
          ->GetFunction(context())
          .ToLocalChecked(),
      NewFunctionTemplate(binding::GetInternalBinding)
          ->GetFunction(context())
          .ToLocalChecked(),
      primordials()};

  // Bootstrap internal loaders
  Local<Value> loader_exports;
  if (!ExecuteBootstrapper(
           this, "internal/bootstrap/loaders", &loaders_params, &loaders_args)
           .ToLocal(&loader_exports)) {
    return MaybeLocal<Value>();
  }
  CHECK(loader_exports->IsObject());
  Local<Object> loader_exports_obj = loader_exports.As<Object>();
  Local<Value> internal_binding_loader =
      loader_exports_obj->Get(context(), internal_binding_string())
          .ToLocalChecked();
  CHECK(internal_binding_loader->IsFunction());
  set_internal_binding_loader(internal_binding_loader.As<Function>());
  Local<Value> require =
      loader_exports_obj->Get(context(), require_string()).ToLocalChecked();
  CHECK(require->IsFunction());
  set_native_module_require(require.As<Function>());

  return scope.Escape(loader_exports);
}

MaybeLocal<Value> Environment::BootstrapNode() {
  EscapableHandleScope scope(isolate_);

  Local<Object> global = context()->Global();
  // TODO(joyeecheung): this can be done in JS land now.
  global->Set(context(), FIXED_ONE_BYTE_STRING(isolate_, "global"), global)
      .Check();

  // process, require, internalBinding, primordials
  std::vector<Local<String>> node_params = {
      process_string(),
      require_string(),
      internal_binding_string(),
      primordials_string()};
  std::vector<Local<Value>> node_args = {
      process_object(),
      native_module_require(),
      internal_binding_loader(),
      primordials()};

  MaybeLocal<Value> result = ExecuteBootstrapper(
      this, "internal/bootstrap/node", &node_params, &node_args);

  if (result.IsEmpty()) {
    return scope.EscapeMaybe(result);
  }

  auto thread_switch_id =
      is_main_thread() ? "internal/bootstrap/switches/is_main_thread"
                       : "internal/bootstrap/switches/is_not_main_thread";
  result =
      ExecuteBootstrapper(this, thread_switch_id, &node_params, &node_args);

  if (result.IsEmpty()) {
    return scope.EscapeMaybe(result);
  }

  auto process_state_switch_id =
      owns_process_state()
          ? "internal/bootstrap/switches/does_own_process_state"
          : "internal/bootstrap/switches/does_not_own_process_state";
  result = ExecuteBootstrapper(
      this, process_state_switch_id, &node_params, &node_args);

  if (result.IsEmpty()) {
    return scope.EscapeMaybe(result);
  }

  Local<String> env_string = FIXED_ONE_BYTE_STRING(isolate_, "env");
  Local<Object> env_var_proxy;
  if (!CreateEnvVarProxy(context(), isolate_, as_callback_data())
           .ToLocal(&env_var_proxy) ||
      process_object()->Set(context(), env_string, env_var_proxy).IsNothing()) {
    return MaybeLocal<Value>();
  }

  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> Environment::RunBootstrapping() {
  EscapableHandleScope scope(isolate_);

  CHECK(!has_run_bootstrapping_code());

  if (BootstrapInternalLoaders().IsEmpty()) {
    return MaybeLocal<Value>();
  }

  Local<Value> result;
  if (!BootstrapNode().ToLocal(&result)) {
    return MaybeLocal<Value>();
  }

  // Make sure that no request or handle is created during bootstrap -
  // if necessary those should be done in pre-execution.
  // Usually, doing so would trigger the checks present in the ReqWrap and
  // HandleWrap classes, so this is only a consistency check.
  CHECK(req_wrap_queue()->IsEmpty());
  CHECK(handle_wrap_queue()->IsEmpty());

  set_has_run_bootstrapping_code(true);

  return scope.Escape(result);
}

void MarkBootstrapComplete(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->performance_state()->Mark(
      performance::NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
}

static
MaybeLocal<Value> StartExecution(Environment* env, const char* main_script_id) {
  EscapableHandleScope scope(env->isolate());
  CHECK_NOT_NULL(main_script_id);

  std::vector<Local<String>> parameters = {
      env->process_string(),
      env->require_string(),
      env->internal_binding_string(),
      env->primordials_string(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "markBootstrapComplete")};

  std::vector<Local<Value>> arguments = {
      env->process_object(),
      env->native_module_require(),
      env->internal_binding_loader(),
      env->primordials(),
      env->NewFunctionTemplate(MarkBootstrapComplete)
          ->GetFunction(env->context())
          .ToLocalChecked()};

  return scope.EscapeMaybe(
      ExecuteBootstrapper(env, main_script_id, &parameters, &arguments));
}

MaybeLocal<Value> StartExecution(Environment* env, StartExecutionCallback cb) {
  InternalCallbackScope callback_scope(
      env,
      Object::New(env->isolate()),
      { 1, 0 },
      InternalCallbackScope::kSkipAsyncHooks);

  if (cb != nullptr) {
    EscapableHandleScope scope(env->isolate());

    if (StartExecution(env, "internal/bootstrap/environment").IsEmpty())
      return {};

    StartExecutionCallbackInfo info = {
      env->process_object(),
      env->native_module_require(),
    };

    return scope.EscapeMaybe(cb(info));
  }

  // To allow people to extend Node in different ways, this hook allows
  // one to drop a file lib/_third_party_main.js into the build
  // directory which will be executed instead of Node's normal loading.
  if (NativeModuleEnv::Exists("_third_party_main")) {
    return StartExecution(env, "internal/main/run_third_party_main");
  }

  if (env->worker_context() != nullptr) {
    return StartExecution(env, "internal/main/worker_thread");
  }

  std::string first_argv;
  if (env->argv().size() > 1) {
    first_argv = env->argv()[1];
  }

  if (first_argv == "inspect" || first_argv == "debug") {
    return StartExecution(env, "internal/main/inspect");
  }

  if (per_process::cli_options->print_help) {
    return StartExecution(env, "internal/main/print_help");
  }


  if (env->options()->prof_process) {
    return StartExecution(env, "internal/main/prof_process");
  }

  // -e/--eval without -i/--interactive
  if (env->options()->has_eval_string && !env->options()->force_repl) {
    return StartExecution(env, "internal/main/eval_string");
  }

  if (env->options()->syntax_check_only) {
    return StartExecution(env, "internal/main/check_syntax");
  }

  if (!first_argv.empty() && first_argv != "-") {
    return StartExecution(env, "internal/main/run_main_module");
  }

  if (env->options()->force_repl || uv_guess_handle(STDIN_FILENO) == UV_TTY) {
    return StartExecution(env, "internal/main/repl");
  }

  return StartExecution(env, "internal/main/eval_stdin");
}

#ifdef __POSIX__
typedef void (*sigaction_cb)(int signo, siginfo_t* info, void* ucontext);
#endif
#if NODE_USE_V8_WASM_TRAP_HANDLER
static std::atomic<sigaction_cb> previous_sigsegv_action;

void TrapWebAssemblyOrContinue(int signo, siginfo_t* info, void* ucontext) {
  if (!v8::TryHandleWebAssemblyTrapPosix(signo, info, ucontext)) {
    sigaction_cb prev = previous_sigsegv_action.load();
    if (prev != nullptr) {
      prev(signo, info, ucontext);
    } else {
      // Reset to the default signal handler, i.e. cause a hard crash.
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = SIG_DFL;
      CHECK_EQ(sigaction(signo, &sa, nullptr), 0);

      ResetStdio();
      raise(signo);
    }
  }
}
#endif  // NODE_USE_V8_WASM_TRAP_HANDLER

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

#endif  // __POSIX__

#ifdef __POSIX__
static struct {
  int flags;
  bool isatty;
  struct stat stat;
  struct termios termios;
} stdio[1 + STDERR_FILENO];
#endif  // __POSIX__


inline void PlatformInit() {
#ifdef __POSIX__
#if HAVE_INSPECTOR
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  const int err = pthread_sigmask(SIG_SETMASK, &sigmask, nullptr);
#endif  // HAVE_INSPECTOR

  // Make sure file descriptors 0-2 are valid before we start logging anything.
  for (auto& s : stdio) {
    const int fd = &s - stdio;
    if (fstat(fd, &s.stat) == 0)
      continue;
    // Anything but EBADF means something is seriously wrong.  We don't
    // have to special-case EINTR, fstat() is not interruptible.
    if (errno != EBADF)
      ABORT();
    if (fd != open("/dev/null", O_RDWR))
      ABORT();
    if (fstat(fd, &s.stat) != 0)
      ABORT();
  }

#if HAVE_INSPECTOR
  CHECK_EQ(err, 0);
#endif  // HAVE_INSPECTOR

  // TODO(addaleax): NODE_SHARED_MODE does not really make sense here.
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
    act.sa_handler = (nr == SIGPIPE || nr == SIGXFSZ) ? SIG_IGN : SIG_DFL;
    CHECK_EQ(0, sigaction(nr, &act, nullptr));
  }
#endif  // !NODE_SHARED_MODE

  // Record the state of the stdio file descriptors so we can restore it
  // on exit.  Needs to happen before installing signal handlers because
  // they make use of that information.
  for (auto& s : stdio) {
    const int fd = &s - stdio;
    int err;

    do
      s.flags = fcntl(fd, F_GETFL);
    while (s.flags == -1 && errno == EINTR);  // NOLINT
    CHECK_NE(s.flags, -1);

    if (uv_guess_handle(fd) != UV_TTY) continue;
    s.isatty = true;

    do
      err = tcgetattr(fd, &s.termios);
    while (err == -1 && errno == EINTR);  // NOLINT
    CHECK_EQ(err, 0);
  }

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


// Safe to call more than once and from signal handlers.
void ResetStdio() {
  uv_tty_reset_mode();
#ifdef __POSIX__
  for (auto& s : stdio) {
    const int fd = &s - stdio;

    struct stat tmp;
    if (-1 == fstat(fd, &tmp)) {
      CHECK_EQ(errno, EBADF);  // Program closed file descriptor.
      continue;
    }

    bool is_same_file =
        (s.stat.st_dev == tmp.st_dev && s.stat.st_ino == tmp.st_ino);
    if (!is_same_file) continue;  // Program reopened file descriptor.

    int flags;
    do
      flags = fcntl(fd, F_GETFL);
    while (flags == -1 && errno == EINTR);  // NOLINT
    CHECK_NE(flags, -1);

    // Restore the O_NONBLOCK flag if it changed.
    if (O_NONBLOCK & (flags ^ s.flags)) {
      flags &= ~O_NONBLOCK;
      flags |= s.flags & O_NONBLOCK;

      int err;
      do
        err = fcntl(fd, F_SETFL, flags);
      while (err == -1 && errno == EINTR);  // NOLINT
      CHECK_NE(err, -1);
    }

    if (s.isatty) {
      sigset_t sa;
      int err;

      // We might be a background job that doesn't own the TTY so block SIGTTOU
      // before making the tcsetattr() call, otherwise that signal suspends us.
      sigemptyset(&sa);
      sigaddset(&sa, SIGTTOU);

      CHECK_EQ(0, pthread_sigmask(SIG_BLOCK, &sa, nullptr));
      do
        err = tcsetattr(fd, TCSANOW, &s.termios);
      while (err == -1 && errno == EINTR);  // NOLINT
      CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &sa, nullptr));

      // Normally we expect err == 0. But if macOS App Sandbox is enabled,
      // tcsetattr will fail with err == -1 and errno == EPERM.
      CHECK_IMPLIES(err != 0, err == -1 && errno == EPERM);
    }
  }
#endif  // __POSIX__
}


int ProcessGlobalArgs(std::vector<std::string>* args,
                      std::vector<std::string>* exec_args,
                      std::vector<std::string>* errors,
                      OptionEnvvarSettings settings) {
  // Parse a few arguments which are specific to Node.
  std::vector<std::string> v8_args;

  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  options_parser::Parse(
      args,
      exec_args,
      &v8_args,
      per_process::cli_options.get(),
      settings,
      errors);

  if (!errors->empty()) return 9;

  std::string revert_error;
  for (const std::string& cve : per_process::cli_options->security_reverts) {
    Revert(cve.c_str(), &revert_error);
    if (!revert_error.empty()) {
      errors->emplace_back(std::move(revert_error));
      return 12;
    }
  }

  if (per_process::cli_options->disable_proto != "delete" &&
      per_process::cli_options->disable_proto != "throw" &&
      per_process::cli_options->disable_proto != "") {
    errors->emplace_back("invalid mode passed to --disable-proto");
    return 12;
  }

  auto env_opts = per_process::cli_options->per_isolate->per_env;
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
    per_process::v8_is_profiling = true;
  }

#ifdef __POSIX__
  // Block SIGPROF signals when sleeping in epoll_wait/kevent/etc.  Avoids the
  // performance penalty of frequent EINTR wakeups when the profiler is running.
  // Only do this for v8.log profiling, as it breaks v8::CpuProfiler users.
  if (per_process::v8_is_profiling) {
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
  for (size_t i = 1; i < v8_args_as_char_ptr.size(); i++)
    errors->push_back("bad option: " + std::string(v8_args_as_char_ptr[i]));

  if (v8_args_as_char_ptr.size() > 1) return 9;

  return 0;
}

static std::atomic_bool init_called{false};

int InitializeNodeWithArgs(std::vector<std::string>* argv,
                           std::vector<std::string>* exec_argv,
                           std::vector<std::string>* errors) {
  // Make sure InitializeNodeWithArgs() is called only once.
  CHECK(!init_called.exchange(true));

  // Initialize node_start_time to get relative uptime.
  per_process::node_start_time = uv_hrtime();

  // Register built-in modules
  binding::RegisterBuiltinModules();

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // Cache the original command line to be
  // used in diagnostic reports.
  per_process::cli_options->cmdline = *argv;

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

  HandleEnvOptions(per_process::cli_options->per_isolate->per_env);

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  std::string node_options;

  if (credentials::SafeGetenv("NODE_OPTIONS", &node_options)) {
    std::vector<std::string> env_argv =
        ParseNodeOptionsEnvVar(node_options, errors);

    if (!errors->empty()) return 9;

    // [0] is expected to be the program name, fill it in from the real argv.
    env_argv.insert(env_argv.begin(), argv->at(0));

    const int exit_code = ProcessGlobalArgs(&env_argv,
                                            nullptr,
                                            errors,
                                            kAllowedInEnvironment);
    if (exit_code != 0) return exit_code;
  }
#endif

  const int exit_code = ProcessGlobalArgs(argv,
                                          exec_argv,
                                          errors,
                                          kDisallowedInEnvironment);
  if (exit_code != 0) return exit_code;

  // Set the process.title immediately after processing argv if --title is set.
  if (!per_process::cli_options->title.empty())
    uv_set_process_title(per_process::cli_options->title.c_str());

#if defined(NODE_HAVE_I18N_SUPPORT)
  // If the parameter isn't given, use the env variable.
  if (per_process::cli_options->icu_data_dir.empty())
    credentials::SafeGetenv("NODE_ICU_DATA",
                            &per_process::cli_options->icu_data_dir);

#ifdef NODE_ICU_DEFAULT_DATA_DIR
  // If neither the CLI option nor the environment variable was specified,
  // fall back to the configured default
  if (per_process::cli_options->icu_data_dir.empty()) {
    // Check whether the NODE_ICU_DEFAULT_DATA_DIR contains the right data
    // file and can be read.
    static const char full_path[] =
        NODE_ICU_DEFAULT_DATA_DIR "/" U_ICUDATA_NAME ".dat";

    FILE* f = fopen(full_path, "rb");

    if (f != nullptr) {
      fclose(f);
      per_process::cli_options->icu_data_dir = NODE_ICU_DEFAULT_DATA_DIR;
    }
  }
#endif  // NODE_ICU_DEFAULT_DATA_DIR

  // Initialize ICU.
  // If icu_data_dir is empty here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(per_process::cli_options->icu_data_dir)) {
    errors->push_back("could not initialize ICU "
                      "(check NODE_ICU_DATA or --icu-data-dir parameters)\n");
    return 9;
  }
  per_process::metadata.versions.InitializeIntlVersions();
#endif

  NativeModuleEnv::InitializeCodeCache();

  // We should set node_is_initialized here instead of in node::Start,
  // otherwise embedders using node::Init to initialize everything will not be
  // able to set it and native modules will not load for them.
  node_is_initialized = true;
  return 0;
}

// TODO(addaleax): Deprecate and eventually remove this.
void Init(int* argc,
          const char** argv,
          int* exec_argc,
          const char*** exec_argv) {
  std::vector<std::string> argv_(argv, argv + *argc);  // NOLINT
  std::vector<std::string> exec_argv_;
  std::vector<std::string> errors;

  // This (approximately) duplicates some logic that has been moved to
  // node::Start(), with the difference that here we explicitly call `exit()`.
  int exit_code = InitializeNodeWithArgs(&argv_, &exec_argv_, &errors);

  for (const std::string& error : errors)
    fprintf(stderr, "%s: %s\n", argv_.at(0).c_str(), error.c_str());
  if (exit_code != 0) exit(exit_code);

  if (per_process::cli_options->print_version) {
    printf("%s\n", NODE_VERSION);
    exit(0);
  }

  if (per_process::cli_options->print_bash_completion) {
    std::string completion = options_parser::GetBashCompletion();
    printf("%s\n", completion.c_str());
    exit(0);
  }

  if (per_process::cli_options->print_v8_help) {
    V8::SetFlagsFromString("--help", 6);  // Doesn't return.
    UNREACHABLE();
  }

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

InitializationResult InitializeOncePerProcess(int argc, char** argv) {
  // Initialized the enabled list for Debug() calls with system
  // environment variables.
  per_process::enabled_debug_list.Parse(nullptr);

  atexit(ResetStdio);
  PlatformInit();

  CHECK_GT(argc, 0);

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

  InitializationResult result;
  result.args = std::vector<std::string>(argv, argv + argc);
  std::vector<std::string> errors;

  // This needs to run *before* V8::Initialize().
  {
    result.exit_code =
        InitializeNodeWithArgs(&(result.args), &(result.exec_args), &errors);
    for (const std::string& error : errors)
      fprintf(stderr, "%s: %s\n", result.args.at(0).c_str(), error.c_str());
    if (result.exit_code != 0) {
      result.early_return = true;
      return result;
    }
  }

  if (per_process::cli_options->use_largepages == "on" ||
      per_process::cli_options->use_largepages == "silent") {
    int result = node::MapStaticCodeToLargePages();
    if (per_process::cli_options->use_largepages == "on" && result != 0) {
      fprintf(stderr, "%s\n", node::LargePagesError(result));
    }
  }

  if (per_process::cli_options->print_version) {
    printf("%s\n", NODE_VERSION);
    result.exit_code = 0;
    result.early_return = true;
    return result;
  }

  if (per_process::cli_options->print_bash_completion) {
    std::string completion = options_parser::GetBashCompletion();
    printf("%s\n", completion.c_str());
    exit(0);
  }

  if (per_process::cli_options->print_v8_help) {
    V8::SetFlagsFromString("--help", 6);  // Doesn't return.
    UNREACHABLE();
  }

#if HAVE_OPENSSL
  {
    std::string extra_ca_certs;
    if (credentials::SafeGetenv("NODE_EXTRA_CA_CERTS", &extra_ca_certs))
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

  per_process::v8_platform.Initialize(
      per_process::cli_options->v8_thread_pool_size);
  V8::Initialize();
  performance::performance_v8_start = PERFORMANCE_NOW();
  per_process::v8_initialized = true;
  return result;
}

void TearDownOncePerProcess() {
  per_process::v8_initialized = false;
  V8::Dispose();

  // uv_run cannot be called from the time before the beforeExit callback
  // runs until the program exits unless the event loop has any referenced
  // handles after beforeExit terminates. This prevents unrefed timers
  // that happen to terminate during shutdown from being run unsafely.
  // Since uv_run cannot be called, uv_async handles held by the platform
  // will never be fully cleaned up.
  per_process::v8_platform.Dispose();
}

int Start(int argc, char** argv) {
  InitializationResult result = InitializeOncePerProcess(argc, argv);
  if (result.early_return) {
    return result.exit_code;
  }

  {
    Isolate::CreateParams params;
    const std::vector<size_t>* indexes = nullptr;
    std::vector<intptr_t> external_references;

    bool force_no_snapshot =
        per_process::cli_options->per_isolate->no_node_snapshot;
    if (!force_no_snapshot) {
      v8::StartupData* blob = NodeMainInstance::GetEmbeddedSnapshotBlob();
      if (blob != nullptr) {
        // TODO(joyeecheung): collect external references and set it in
        // params.external_references.
        external_references.push_back(reinterpret_cast<intptr_t>(nullptr));
        params.external_references = external_references.data();
        params.snapshot_blob = blob;
        indexes = NodeMainInstance::GetIsolateDataIndexes();
      }
    }
    uv_loop_configure(uv_default_loop(), UV_METRICS_IDLE_TIME);

    NodeMainInstance main_instance(&params,
                                   uv_default_loop(),
                                   per_process::v8_platform.Platform(),
                                   result.args,
                                   result.exec_args,
                                   indexes);
    result.exit_code = main_instance.Run();
  }

  TearDownOncePerProcess();
  return result.exit_code;
}

int Stop(Environment* env) {
  env->ExitEnv();
  return 0;
}

}  // namespace node

#if !HAVE_INSPECTOR
void Initialize() {}

NODE_MODULE_CONTEXT_AWARE_INTERNAL(inspector, Initialize)
#endif  // !HAVE_INSPECTOR

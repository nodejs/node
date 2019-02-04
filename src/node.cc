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

#include "node_binding.h"
#include "node_buffer.h"
#include "node_constants.h"
#include "node_context_data.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_metadata.h"
#include "node_native_module.h"
#include "node_options-inl.h"
#include "node_perf.h"
#include "node_platform.h"
#include "node_process.h"
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
#include "inspector_io.h"
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
#include "node_dtrace.h"
#endif

#include "async_wrap-inl.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "req_wrap-inl.h"
#include "string_bytes.h"
#include "util.h"
#include "uv.h"
#if NODE_USE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif  // NODE_USE_V8_PLATFORM
#include "v8-profiler.h"

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

#ifdef NODE_REPORT
#include "node_report.h"
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
#include <unistd.h>        // STDIN_FILENO, STDERR_FILENO
#endif

namespace node {

using options_parser::kAllowedInEnvironment;
using options_parser::kDisallowedInEnvironment;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Locker;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Object;
using v8::Script;
using v8::SealHandleScope;
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
// process-relative uptime base, initialized at start-up
double prog_start_time;
// Tells whether --prof is passed.
bool v8_is_profiling = false;

// node_v8_platform-inl.h
struct V8Platform v8_platform;
}  // namespace per_process

#ifdef __POSIX__
static const unsigned kMaxSignal = 32;
#endif

void WaitForInspectorDisconnect(Environment* env) {
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

static MaybeLocal<Value> ExecuteBootstrapper(
    Environment* env,
    const char* id,
    std::vector<Local<String>>* parameters,
    std::vector<Local<Value>>* arguments) {
  EscapableHandleScope scope(env->isolate());
  MaybeLocal<Function> maybe_fn =
      per_process::native_module_loader.LookupAndCompile(
          env->context(), id, parameters, env);

  if (maybe_fn.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  Local<Function> fn = maybe_fn.ToLocalChecked();
  MaybeLocal<Value> result = fn->Call(env->context(),
                                      Undefined(env->isolate()),
                                      arguments->size(),
                                      arguments->data());

  // If there was an error during bootstrap then it was either handled by the
  // FatalException handler or it's unrecoverable (e.g. max call stack
  // exceeded). Either way, clear the stack so that the AsyncCallbackScope
  // destructor doesn't fail on the id check.
  // There are only two ways to have a stack size > 1: 1) the user manually
  // called MakeCallback or 2) user awaited during bootstrap, which triggered
  // _tickCallback().
  if (result.IsEmpty()) {
    env->async_hooks()->clear_async_id_stack();
  }

  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> RunBootstrapping(Environment* env) {
  CHECK(!env->has_run_bootstrapping_code());

  EscapableHandleScope scope(env->isolate());
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  // Add a reference to the global object
  Local<Object> global = context->Global();

#if defined HAVE_DTRACE || defined HAVE_ETW
  InitDTrace(env, global);
#endif

  Local<Object> process = env->process_object();

  // Setting global properties for the bootstrappers to use:
  // - global
  // Expose the global object as a property on itself
  // (Allows you to set stuff on `global` from anywhere in JavaScript.)
  global->Set(context, FIXED_ONE_BYTE_STRING(env->isolate(), "global"), global)
      .FromJust();

  // Store primordials
  env->set_primordials(Object::New(isolate));
  std::vector<Local<String>> primordials_params = {
    FIXED_ONE_BYTE_STRING(isolate, "breakAtBootstrap"),
    env->primordials_string()
  };
  std::vector<Local<Value>> primordials_args = {
    Boolean::New(isolate,
                  env->options()->debug_options().break_node_first_line),
    env->primordials()
  };
  MaybeLocal<Value> primordials_ret =
      ExecuteBootstrapper(env,
                          "internal/bootstrap/primordials",
                          &primordials_params,
                          &primordials_args);
  if (primordials_ret.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  // Create binding loaders
  std::vector<Local<String>> loaders_params = {
      env->process_string(),
      FIXED_ONE_BYTE_STRING(isolate, "getLinkedBinding"),
      FIXED_ONE_BYTE_STRING(isolate, "getInternalBinding"),
      // --experimental-modules
      FIXED_ONE_BYTE_STRING(isolate, "experimentalModules"),
      // --expose-internals
      FIXED_ONE_BYTE_STRING(isolate, "exposeInternals"),
      env->primordials_string()};
  std::vector<Local<Value>> loaders_args = {
      process,
      env->NewFunctionTemplate(binding::GetLinkedBinding)
          ->GetFunction(context)
          .ToLocalChecked(),
      env->NewFunctionTemplate(binding::GetInternalBinding)
          ->GetFunction(context)
          .ToLocalChecked(),
      Boolean::New(isolate, env->options()->experimental_modules),
      Boolean::New(isolate, env->options()->expose_internals),
      env->primordials()};

  // Bootstrap internal loaders
  MaybeLocal<Value> loader_exports = ExecuteBootstrapper(
      env, "internal/bootstrap/loaders", &loaders_params, &loaders_args);
  if (loader_exports.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  Local<Object> loader_exports_obj =
      loader_exports.ToLocalChecked().As<Object>();
  Local<Value> internal_binding_loader =
      loader_exports_obj->Get(context, env->internal_binding_string())
          .ToLocalChecked();
  env->set_internal_binding_loader(internal_binding_loader.As<Function>());

  Local<Value> require =
      loader_exports_obj->Get(context, env->require_string()).ToLocalChecked();
  env->set_native_module_require(require.As<Function>());

  // process, loaderExports, isMainThread, ownsProcessState, primordials
  std::vector<Local<String>> node_params = {
      env->process_string(),
      FIXED_ONE_BYTE_STRING(isolate, "loaderExports"),
      FIXED_ONE_BYTE_STRING(isolate, "isMainThread"),
      FIXED_ONE_BYTE_STRING(isolate, "ownsProcessState"),
      env->primordials_string()};
  std::vector<Local<Value>> node_args = {
      process,
      loader_exports_obj,
      Boolean::New(isolate, env->is_main_thread()),
      Boolean::New(isolate, env->owns_process_state()),
      env->primordials()};

  MaybeLocal<Value> result = ExecuteBootstrapper(
      env, "internal/bootstrap/node", &node_params, &node_args);

  env->set_has_run_bootstrapping_code(true);

  return scope.EscapeMaybe(result);
}

void MarkBootstrapComplete(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->performance_state()->Mark(
      performance::NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
}

MaybeLocal<Value> StartExecution(Environment* env, const char* main_script_id) {
  EscapableHandleScope scope(env->isolate());
  CHECK_NOT_NULL(main_script_id);

  std::vector<Local<String>> parameters = {
      env->process_string(),
      env->require_string(),
      env->internal_binding_string(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "markBootstrapComplete")};

  std::vector<Local<Value>> arguments = {
      env->process_object(),
      env->native_module_require(),
      env->internal_binding_loader(),
      env->NewFunctionTemplate(MarkBootstrapComplete)
          ->GetFunction(env->context())
          .ToLocalChecked()};

  MaybeLocal<Value> result =
      ExecuteBootstrapper(env, main_script_id, &parameters, &arguments);
  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> StartMainThreadExecution(Environment* env) {
  // To allow people to extend Node in different ways, this hook allows
  // one to drop a file lib/_third_party_main.js into the build
  // directory which will be executed instead of Node's normal loading.
  if (per_process::native_module_loader.Exists("_third_party_main")) {
    return StartExecution(env, "internal/main/run_third_party_main");
  }

  if (env->execution_mode() == Environment::ExecutionMode::kInspect ||
      env->execution_mode() == Environment::ExecutionMode::kDebug) {
    return StartExecution(env, "internal/main/inspect");
  }

  if (per_process::cli_options->print_help) {
    env->set_execution_mode(Environment::ExecutionMode::kPrintHelp);
    return StartExecution(env, "internal/main/print_help");
  }

  if (per_process::cli_options->print_bash_completion) {
    env->set_execution_mode(Environment::ExecutionMode::kPrintBashCompletion);
    return StartExecution(env, "internal/main/print_bash_completion");
  }

  if (env->options()->prof_process) {
    env->set_execution_mode(Environment::ExecutionMode::kPrintBashCompletion);
    return StartExecution(env, "internal/main/prof_process");
  }

  // -e/--eval without -i/--interactive
  if (env->options()->has_eval_string && !env->options()->force_repl) {
    env->set_execution_mode(Environment::ExecutionMode::kEvalString);
    return StartExecution(env, "internal/main/eval_string");
  }

  if (env->options()->syntax_check_only) {
    env->set_execution_mode(Environment::ExecutionMode::kCheckSyntax);
    return StartExecution(env, "internal/main/check_syntax");
  }

  if (env->execution_mode() == Environment::ExecutionMode::kRunMainModule) {
    return StartExecution(env, "internal/main/run_main_module");
  }

  if (env->options()->force_repl || uv_guess_handle(STDIN_FILENO) == UV_TTY) {
    env->set_execution_mode(Environment::ExecutionMode::kRepl);
    return StartExecution(env, "internal/main/repl");
  }

  env->set_execution_mode(Environment::ExecutionMode::kEvalStdin);
  return StartExecution(env, "internal/main/eval_stdin");
}

void LoadEnvironment(Environment* env) {
  CHECK(env->is_main_thread());
  // TODO(joyeecheung): Not all of the execution modes in
  // StartMainThreadExecution() make sense for embedders. Pick the
  // useful ones out, and allow embedders to customize the entry
  // point more directly without using _third_party_main.js
  if (!RunBootstrapping(env).IsEmpty()) {
    USE(StartMainThreadExecution(env));
  }
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

int ProcessGlobalArgs(std::vector<std::string>* args,
                      std::vector<std::string>* exec_args,
                      std::vector<std::string>* errors,
                      bool is_env) {
  // Parse a few arguments which are specific to Node.
  std::vector<std::string> v8_args;

  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  options_parser::PerProcessOptionsParser::instance.Parse(
      args,
      exec_args,
      &v8_args,
      per_process::cli_options.get(),
      is_env ? kAllowedInEnvironment : kDisallowedInEnvironment,
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

int Init(std::vector<std::string>* argv,
         std::vector<std::string>* exec_argv,
         std::vector<std::string>* errors) {
  // Initialize prog_start_time to get relative uptime.
  per_process::prog_start_time = static_cast<double>(uv_now(uv_default_loop()));

  // Register built-in modules
  binding::RegisterBuiltinModules();

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

#ifdef NODE_REPORT
  // Cache the original command line to be
  // used in diagnostic reports.
  per_process::cli_options->cmdline = *argv;
#endif  //  NODE_REPORT

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

  std::shared_ptr<EnvironmentOptions> default_env_options =
      per_process::cli_options->per_isolate->per_env;
  {
    std::string text;
    default_env_options->pending_deprecation =
        credentials::SafeGetenv("NODE_PENDING_DEPRECATION", &text) &&
        text[0] == '1';
  }

  // Allow for environment set preserving symlinks.
  {
    std::string text;
    default_env_options->preserve_symlinks =
        credentials::SafeGetenv("NODE_PRESERVE_SYMLINKS", &text) &&
        text[0] == '1';
  }

  {
    std::string text;
    default_env_options->preserve_symlinks_main =
        credentials::SafeGetenv("NODE_PRESERVE_SYMLINKS_MAIN", &text) &&
        text[0] == '1';
  }

  if (default_env_options->redirect_warnings.empty()) {
    credentials::SafeGetenv("NODE_REDIRECT_WARNINGS",
                            &default_env_options->redirect_warnings);
  }

#if HAVE_OPENSSL
  std::string* openssl_config = &per_process::cli_options->openssl_config;
  if (openssl_config->empty()) {
    credentials::SafeGetenv("OPENSSL_CONF", openssl_config);
  }
#endif

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  std::string node_options;
  if (credentials::SafeGetenv("NODE_OPTIONS", &node_options)) {
    // [0] is expected to be the program name, fill it in from the real argv
    // and use 'x' as a placeholder while parsing.
    std::vector<std::string> env_argv = SplitString("x " + node_options, ' ');
    env_argv[0] = argv->at(0);

    const int exit_code = ProcessGlobalArgs(&env_argv, nullptr, errors, true);
    if (exit_code != 0) return exit_code;
  }
#endif

  const int exit_code = ProcessGlobalArgs(argv, exec_argv, errors, false);
  if (exit_code != 0) return exit_code;

  // Set the process.title immediately after processing argv if --title is set.
  if (!per_process::cli_options->title.empty())
    uv_set_process_title(per_process::cli_options->title.c_str());

#if defined(NODE_HAVE_I18N_SUPPORT)
  // If the parameter isn't given, use the env variable.
  if (per_process::cli_options->icu_data_dir.empty())
    credentials::SafeGetenv("NODE_ICU_DATA",
                            &per_process::cli_options->icu_data_dir);
  // Initialize ICU.
  // If icu_data_dir is empty here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(per_process::cli_options->icu_data_dir)) {
    errors->push_back("could not initialize ICU "
                      "(check NODE_ICU_DATA or --icu-data-dir parameters)\n");
    return 9;
  }
  per_process::metadata.versions.InitializeIntlVersions();
#endif

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
  int exit_code = Init(&argv_, &exec_argv_, &errors);

  for (const std::string& error : errors)
    fprintf(stderr, "%s: %s\n", argv_.at(0).c_str(), error.c_str());
  if (exit_code != 0) exit(exit_code);

  if (per_process::cli_options->print_version) {
    printf("%s\n", NODE_VERSION);
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

void RunBeforeExit(Environment* env) {
  env->RunBeforeExitCallbacks();

  if (!uv_loop_alive(env->event_loop()))
    EmitBeforeExit(env);
}

inline int Start(Isolate* isolate, IsolateData* isolate_data,
                 const std::vector<std::string>& args,
                 const std::vector<std::string>& exec_args) {
  HandleScope handle_scope(isolate);
  Local<Context> context = NewContext(isolate);
  Context::Scope context_scope(context);
  Environment env(
      isolate_data,
      context,
      static_cast<Environment::Flags>(Environment::kIsMainThread |
                                      Environment::kOwnsProcessState |
                                      Environment::kOwnsInspector));
  env.Start(per_process::v8_is_profiling);
  env.ProcessCliArgs(args, exec_args);

#if HAVE_INSPECTOR && NODE_USE_V8_PLATFORM
  CHECK(!env.inspector_agent()->IsListening());
  // Inspector agent can't fail to start, but if it was configured to listen
  // right away on the websocket port and fails to bind/etc, this will return
  // false.
  env.inspector_agent()->Start(args.size() > 1 ? args[1].c_str() : "",
                               env.options()->debug_options(),
                               env.inspector_host_port(),
                               true);
  if (env.options()->debug_options().inspector_enabled &&
      !env.inspector_agent()->IsListening()) {
    return 12;  // Signal internal error.
  }
#else
  // inspector_enabled can't be true if !HAVE_INSPECTOR or !NODE_USE_V8_PLATFORM
  // - the option parser should not allow that.
  CHECK(!env.options()->debug_options().inspector_enabled);
#endif  // HAVE_INSPECTOR && NODE_USE_V8_PLATFORM

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

      per_process::v8_platform.DrainVMTasks(isolate);

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

  per_process::v8_platform.DrainVMTasks(isolate);
  per_process::v8_platform.CancelVMTasks(isolate);
#if defined(LEAK_SANITIZER)
  __lsan_do_leak_check();
#endif

  return exit_code;
}

inline int Start(uv_loop_t* event_loop,
                 const std::vector<std::string>& args,
                 const std::vector<std::string>& exec_args) {
  std::unique_ptr<ArrayBufferAllocator, decltype(&FreeArrayBufferAllocator)>
      allocator(CreateArrayBufferAllocator(), &FreeArrayBufferAllocator);
  Isolate* const isolate = NewIsolate(allocator.get(), event_loop);
  if (isolate == nullptr)
    return 12;  // Signal internal error.

  if (per_process::cli_options->print_version) {
    printf("%s\n", NODE_VERSION);
    return 0;
  }

  if (per_process::cli_options->print_v8_help) {
    V8::SetFlagsFromString("--help", 6);  // Doesn't return.
    UNREACHABLE();
  }

  int exit_code;
  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    std::unique_ptr<IsolateData, decltype(&FreeIsolateData)> isolate_data(
        CreateIsolateData(isolate,
                          event_loop,
                          per_process::v8_platform.Platform(),
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

  isolate->Dispose();
  per_process::v8_platform.Platform()->UnregisterIsolate(isolate);

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
  std::vector<std::string> errors;
  // This needs to run *before* V8::Initialize().
  {
    const int exit_code = Init(&args, &exec_args, &errors);
    for (const std::string& error : errors)
      fprintf(stderr, "%s: %s\n", args.at(0).c_str(), error.c_str());
    if (exit_code != 0) return exit_code;
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

  InitializeV8Platform(per_process::cli_options->v8_thread_pool_size);
  V8::Initialize();
  performance::performance_v8_start = PERFORMANCE_NOW();
  per_process::v8_initialized = true;
  const int exit_code =
      Start(uv_default_loop(), args, exec_args);
  per_process::v8_initialized = false;
  V8::Dispose();

  // uv_run cannot be called from the time before the beforeExit callback
  // runs until the program exits unless the event loop has any referenced
  // handles after beforeExit terminates. This prevents unrefed timers
  // that happen to terminate during shutdown from being run unsafely.
  // Since uv_run cannot be called, uv_async handles held by the platform
  // will never be fully cleaned up.
  per_process::v8_platform.Dispose();

  return exit_code;
}

}  // namespace node

#if !HAVE_INSPECTOR
void Initialize() {}

NODE_MODULE_CONTEXT_AWARE_INTERNAL(inspector, Initialize)
#endif  // !HAVE_INSPECTOR

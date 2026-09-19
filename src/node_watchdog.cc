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

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>

#include "async_wrap-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_sockaddr-inl.h"
#include "node_watchdog.h"
#include "node_worker.h"
#include "util-inl.h"

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::StackFrame;
using v8::StackTrace;
using v8::Value;

Watchdog::Watchdog(v8::Isolate* isolate, uint64_t ms, bool* timed_out)
    : isolate_(isolate), timed_out_(timed_out) {

  int rc;
  rc = uv_loop_init(&loop_);
  if (rc != 0) {
    UNREACHABLE("Failed to initialize uv loop.");
  }

  rc = uv_async_init(&loop_, &async_, [](uv_async_t* signal) {
    Watchdog* w = ContainerOf(&Watchdog::async_, signal);
    uv_stop(&w->loop_);
  });

  CHECK_EQ(0, rc);

  rc = uv_timer_init(&loop_, &timer_);
  CHECK_EQ(0, rc);

  rc = uv_timer_start(&timer_, &Watchdog::Timer, ms, 0);
  CHECK_EQ(0, rc);

  rc = uv_thread_create(&thread_, &Watchdog::Run, this);
  CHECK_EQ(0, rc);
}


Watchdog::~Watchdog() {
  uv_async_send(&async_);
  uv_thread_join(&thread_);

  uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);

  // UV_RUN_DEFAULT so that libuv has a chance to clean up.
  uv_run(&loop_, UV_RUN_DEFAULT);

  CheckedUvLoopClose(&loop_);
}


void Watchdog::Run(void* arg) {
  uv_thread_setname("Watchdog");
  Watchdog* wd = static_cast<Watchdog*>(arg);

  // UV_RUN_DEFAULT the loop will be stopped either by the async or the
  // timer handle.
  uv_run(&wd->loop_, UV_RUN_DEFAULT);

  // Loop ref count reaches zero when both handles are closed.
  // Close the timer handle on this side and let ~Watchdog() close async_
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->timer_), nullptr);
}

void Watchdog::Timer(uv_timer_t* timer) {
  Watchdog* w = ContainerOf(&Watchdog::timer_, timer);
  *w->timed_out_ = true;
  w->isolate()->TerminateExecution();
  uv_stop(&w->loop_);
}

namespace {

constexpr uint64_t kNanosecondsPerMillisecond = 1000 * 1000;
// How long the main thread has to respond to the timeout. If it does not, it
// is most likely blocked in a synchronous native call.
constexpr uint64_t kProcessTimeoutResponseGraceMs = 2000;
// How long printing the diagnostics, writing the report and exiting may take.
constexpr uint64_t kProcessTimeoutExitGraceMs = 5000;

std::string FormatProcessTimeoutHeader(const std::string& duration) {
  return SPrintF("(node:%d) Process timed out after %s (--process-timeout). "
                 "Exiting with code %d.\n",
                 uv_os_getpid(),
                 duration,
                 static_cast<int>(ExitCode::kProcessTimeout));
}

// Called from the watchdog thread when the main thread cannot be trusted to
// exit on its own. It must not touch the Environment or V8.
[[noreturn]] void ForceProcessTimeoutExit(const std::string& message) {
  FPrintF(stderr, "%s", message);
  fflush(stderr);
  ResetStdio();
  std::_Exit(static_cast<int>(ExitCode::kProcessTimeout));
}

std::string FormatJavaScriptStack(Isolate* isolate, Local<StackTrace> stack) {
  std::string result;
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> frame = stack->GetFrame(isolate, i);
    Utf8Value function_name(isolate, frame->GetFunctionName());
    Utf8Value script_name(isolate, frame->GetScriptName());
    const int line = frame->GetLineNumber();
    const int column = frame->GetColumn();
    if (function_name.length() == 0) {
      result += SPrintF("    at %s:%d:%d\n", script_name, line, column);
    } else {
      result += SPrintF(
          "    at %s (%s:%d:%d)\n", function_name, script_name, line, column);
    }
  }
  return result;
}

// Describes the details of a libuv handle that help identify where it comes
// from, e.g. the address a server listens on or the pid of a child process.
std::string DescribeHandle(uv_handle_t* handle, std::string_view name) {
  std::string description;
  switch (handle->type) {
    case UV_TCP: {
      const uv_tcp_t& tcp = *reinterpret_cast<uv_tcp_t*>(handle);
      const std::string local = SocketAddress::FromSockName(tcp).ToString();
      const std::string remote = SocketAddress::FromPeerName(tcp).ToString();
      if (!remote.empty()) {
        description = local + " -> " + remote;
      } else if (!local.empty()) {
        description =
            (name == "TCPServerWrap" ? "listening on " : "local ") + local;
      }
      break;
    }
    case UV_UDP: {
      const uv_udp_t& udp = *reinterpret_cast<uv_udp_t*>(handle);
      const std::string local = SocketAddress::FromSockName(udp).ToString();
      if (!local.empty()) description = "bound to " + local;
      break;
    }
    case UV_PROCESS:
      description =
          SPrintF("pid %d",
                  uv_process_get_pid(reinterpret_cast<uv_process_t*>(handle)));
      break;
    case UV_FS_EVENT:
    case UV_FS_POLL: {
      char path[PATH_MAX_BYTES];
      size_t size = sizeof(path);
      const int rc =
          handle->type == UV_FS_EVENT
              ? uv_fs_event_getpath(
                    reinterpret_cast<uv_fs_event_t*>(handle), path, &size)
              : uv_fs_poll_getpath(
                    reinterpret_cast<uv_fs_poll_t*>(handle), path, &size);
      if (rc == 0) description = "watching " + std::string(path, size);
      break;
    }
    case UV_SIGNAL:
      description =
          signo_string(reinterpret_cast<uv_signal_t*>(handle)->signum);
      break;
    case UV_TIMER:
      description =
          SPrintF("due in %dms",
                  uv_timer_get_due_in(reinterpret_cast<uv_timer_t*>(handle)));
      break;
    default:
      break;
  }

#ifndef _WIN32
  uv_os_fd_t fd;
  if (handle->type != UV_PROCESS && uv_fileno(handle, &fd) == 0) {
    description += (description.empty() ? "" : ", ") + SPrintF("fd %d", fd);
  }
#endif

  return description;
}

// Collects resources in the order in which they are found, merging duplicates.
class ResourceList {
 public:
  void Add(std::string name, std::string details = "", size_t count = 1) {
    for (Entry& entry : entries_) {
      if (entry.name == name && entry.details == details) {
        entry.count += count;
        return;
      }
    }
    entries_.push_back({std::move(name), std::move(details), count});
  }

  std::string ToString() const {
    std::string result;
    for (const Entry& entry : entries_) {
      result += "    " + entry.name;
      if (entry.count > 1) result += SPrintF(" x%d", entry.count);
      if (!entry.details.empty()) result += " (" + entry.details + ")";
      result += "\n";
    }
    return result;
  }

  bool empty() const { return entries_.empty(); }

 private:
  struct Entry {
    std::string name;
    std::string details;
    size_t count;
  };
  std::vector<Entry> entries_;
};

// Describes what the main thread was doing and what keeps the event loop
// alive. This must not call into JavaScript, as it may run from a V8 interrupt.
std::string FormatProcessTimeoutDiagnostics(Environment* env) {
  Isolate* isolate = env->isolate();
  std::string result;

  Local<StackTrace> stack;
  if (GetCurrentStackTrace(isolate, static_cast<int>(env->stack_trace_limit()))
          .ToLocal(&stack) &&
      stack->GetFrameCount() > 0) {
    result += "Main thread was executing JavaScript:\n";
    result += FormatJavaScriptStack(isolate, stack);
  } else {
    result += "Main thread was not executing JavaScript.\n";
  }

  ResourceList resources;

  for (ReqWrapBase* req_wrap : *env->req_wrap_queue()) {
    AsyncWrap* wrap = req_wrap->GetAsyncWrap();
    if (wrap->persistent().IsEmpty()) continue;
    resources.Add(wrap->MemoryInfoName());
  }

  std::unordered_set<const uv_handle_t*> handle_wraps;
  for (HandleWrap* wrap : *env->handle_wrap_queue()) {
    uv_handle_t* handle = wrap->GetHandle();
    handle_wraps.insert(handle);
    if (wrap->persistent().IsEmpty() || !HandleWrap::HasRef(wrap) ||
        !uv_is_active(handle)) {
      continue;
    }
    const std::string name = wrap->MemoryInfoName();
    resources.Add(name, DescribeHandle(handle, name));
  }

  // Timers and immediates are tracked in JavaScript and share one libuv handle
  // per kind, so report how many of them keep the event loop alive.
  const int32_t timeouts = env->timeout_info()[0];
  if (timeouts > 0) {
    // The timer handle is inactive while the timers are being processed.
    uv_timer_t* timer_handle = env->timer_handle();
    resources.Add(
        "Timeout",
        uv_is_active(reinterpret_cast<uv_handle_t*>(timer_handle))
            ? SPrintF("next due in %dms", uv_timer_get_due_in(timer_handle))
            : "",
        timeouts);
  }
  const uint32_t immediates = env->immediate_info()->ref_count();
  if (immediates > 0) resources.Add("Immediate", "", immediates);

  // Running Workers keep the event loop alive through the Environment's own
  // task queue handle rather than through a HandleWrap.
  env->ForEachWorker([&](worker::Worker* worker) {
    if (worker->is_internal() || !worker->has_ref() || worker->is_stopped()) {
      return;
    }
    std::string details = SPrintF("thread %d", worker->thread_id());
    if (!worker->name().empty()) {
      details += SPrintF(", name '%s'", worker->name());
    }
    resources.Add("Worker", std::move(details));
  });

  // Anything else, e.g. handles created by native addons.
  struct WalkData {
    Environment* env;
    const std::unordered_set<const uv_handle_t*>* handle_wraps;
    ResourceList* resources;
  } walk_data{env, &handle_wraps, &resources};
  uv_walk(
      env->event_loop(),
      [](uv_handle_t* handle, void* arg) {
        WalkData* data = static_cast<WalkData*>(arg);
        Environment* env = data->env;
        if (!uv_is_active(handle) || !uv_has_ref(handle) ||
            data->handle_wraps->contains(handle) ||
            handle == reinterpret_cast<uv_handle_t*>(env->timer_handle()) ||
            handle ==
                reinterpret_cast<uv_handle_t*>(env->immediate_idle_handle()) ||
            handle ==
                reinterpret_cast<uv_handle_t*>(env->task_queues_async())) {
          return;
        }
        data->resources->Add("libuv handle", uv_handle_type_name(handle->type));
      },
      &walk_data);

  if (resources.empty()) {
    result += "No resources keeping the event loop alive were found.\n";
  } else {
    result += "Resources keeping the event loop alive:\n";
    result += resources.ToString();
  }
  return result;
}

}  // namespace

struct ProcessTimeoutWatchdog::State {
  enum class Phase {
    // Waiting for the deadline.
    kArmed,
    // The deadline was reached and the main thread was interrupted.
    kFired,
    // The main thread is printing diagnostics and exiting.
    kHandling,
    // The event loop has stopped and the Environment is being torn down.
    kStopping,
    // The process is exiting normally.
    kDisarmed,
  };

  State(uint64_t deadline, std::string duration, bool report)
      : deadline(deadline), duration(std::move(duration)), report(report) {
    CHECK_EQ(uv_mutex_init(&mutex), 0);
    CHECK_EQ(uv_cond_init(&cond), 0);
  }

  ~State() {
    uv_cond_destroy(&cond);
    uv_mutex_destroy(&mutex);
  }

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  // With `mutex` held, waits while the phase is `current`, but at most until
  // uv_hrtime() reaches `until`.
  void WaitWhile(Phase current, uint64_t until) {
    while (phase == current) {
      const uint64_t now = uv_hrtime();
      if (now >= until) return;
      uv_cond_timedwait(&cond, &mutex, until - now);
    }
  }

  uv_mutex_t mutex;
  uv_cond_t cond;
  Phase phase = Phase::kArmed;
  // In uv_hrtime() nanoseconds.
  const uint64_t deadline;
  // As passed to --process-timeout, e.g. "30s".
  const std::string duration;
  // --report-on-process-timeout
  const bool report;
};

bool ProcessTimeoutWatchdog::IsEnabled() {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  return per_process::cli_options->process_timeout_ms != 0;
}

std::unique_ptr<ProcessTimeoutWatchdog> ProcessTimeoutWatchdog::MaybeStart(
    Environment* env) {
  CHECK(env->is_main_thread());
  uint64_t timeout_ms;
  std::string duration;
  bool report;
  {
    Mutex::ScopedLock lock(per_process::cli_options_mutex);
    timeout_ms = per_process::cli_options->process_timeout_ms;
    duration = per_process::cli_options->process_timeout;
    report = per_process::cli_options->report_on_process_timeout;
  }
  if (timeout_ms == 0) return nullptr;

  // In watch mode, this process only restarts the application in child
  // processes, which inherit --process-timeout and apply it to each run.
  if (env->options()->watch_mode) return nullptr;

  const uint64_t deadline =
      per_process::node_start_time + timeout_ms * kNanosecondsPerMillisecond;
  auto state = std::make_shared<State>(deadline, std::move(duration), report);
  return std::unique_ptr<ProcessTimeoutWatchdog>(
      new ProcessTimeoutWatchdog(env, std::move(state)));
}

ProcessTimeoutWatchdog::ProcessTimeoutWatchdog(Environment* env,
                                               std::shared_ptr<State> state)
    : env_(env), state_(std::move(state)) {
  CHECK_EQ(uv_thread_create(&thread_, Run, this), 0);
}

ProcessTimeoutWatchdog::~ProcessTimeoutWatchdog() {
  uv_mutex_lock(&state_->mutex);
  state_->phase = State::Phase::kDisarmed;
  uv_cond_signal(&state_->cond);
  uv_mutex_unlock(&state_->mutex);
  CHECK_EQ(uv_thread_join(&thread_), 0);
}

void ProcessTimeoutWatchdog::OnEnvironmentStopping() {
  uv_mutex_lock(&state_->mutex);
  if (state_->phase == State::Phase::kArmed ||
      state_->phase == State::Phase::kFired) {
    state_->phase = State::Phase::kStopping;
    uv_cond_signal(&state_->cond);
  }
  uv_mutex_unlock(&state_->mutex);
}

void ProcessTimeoutWatchdog::Run(void* arg) {
  uv_thread_setname("ProcessTimeout");
  using Phase = State::Phase;
  ProcessTimeoutWatchdog* self = static_cast<ProcessTimeoutWatchdog*>(arg);
  std::shared_ptr<State> state = self->state_;

  uv_mutex_lock(&state->mutex);
  state->WaitWhile(Phase::kArmed, state->deadline);

  if (state->phase == Phase::kArmed) {
    state->phase = Phase::kFired;
    // The Environment is only freed after OnEnvironmentStopping(), which
    // cannot happen while we hold the mutex. The callback runs from a V8
    // interrupt if JavaScript is executing, or from the event loop otherwise.
    self->env_->RequestInterrupt(
        [state](Environment* env) { OnTimeout(env, state); });

    state->WaitWhile(Phase::kFired,
                     uv_hrtime() + kProcessTimeoutResponseGraceMs *
                                       kNanosecondsPerMillisecond);
    if (state->phase == Phase::kFired) {
      ForceProcessTimeoutExit(
          FormatProcessTimeoutHeader(state->duration) +
          SPrintF("The main thread did not respond within %dms. It is likely "
                  "blocked in a synchronous native operation, e.g. "
                  "child_process.execSync() or a native addon, so no "
                  "JavaScript stack or resource information is available.\n",
                  kProcessTimeoutResponseGraceMs));
    }

    if (state->phase == Phase::kHandling) {
      state->WaitWhile(Phase::kHandling,
                       uv_hrtime() + kProcessTimeoutExitGraceMs *
                                         kNanosecondsPerMillisecond);
      if (state->phase == Phase::kHandling) {
        ForceProcessTimeoutExit(
            SPrintF("(node:%d) The process did not finish exiting within %dms "
                    "after --process-timeout expired. Forcing exit.\n",
                    uv_os_getpid(),
                    kProcessTimeoutExitGraceMs));
      }
    }
  }

  if (state->phase == Phase::kStopping) {
    // The event loop has stopped, but tearing down the Environment, e.g.
    // joining Worker threads, can still take arbitrarily long. If the deadline
    // has already passed, give the process a moment to finish exiting.
    const uint64_t now = uv_hrtime();
    const uint64_t until =
        now < state->deadline
            ? state->deadline
            : now + kProcessTimeoutResponseGraceMs * kNanosecondsPerMillisecond;
    state->WaitWhile(Phase::kStopping, until);
    if (state->phase == Phase::kStopping) {
      ForceProcessTimeoutExit(FormatProcessTimeoutHeader(state->duration) +
                              "The process did not finish exiting after the "
                              "event loop had stopped.\n");
    }
  }

  uv_mutex_unlock(&state->mutex);
}

void ProcessTimeoutWatchdog::OnTimeout(Environment* env,
                                       const std::shared_ptr<State>& state) {
  uv_mutex_lock(&state->mutex);
  const bool should_handle = state->phase == State::Phase::kFired;
  if (should_handle) {
    state->phase = State::Phase::kHandling;
    uv_cond_signal(&state->cond);
  }
  uv_mutex_unlock(&state->mutex);
  if (!should_handle) return;

  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  {
    Isolate::DisallowJavascriptExecutionScope disallow_js(
        isolate, Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);
    FPrintF(stderr,
            "%s%s",
            FormatProcessTimeoutHeader(state->duration),
            FormatProcessTimeoutDiagnostics(env));
    fflush(stderr);

    if (state->report) {
      TriggerNodeReport(env,
                        "Process timed out (--process-timeout)",
                        "ProcessTimeout",
                        "",
                        Local<Value>());
    }
  }

  // Like process.reallyExit(): flush coverage and profiles, but do not emit
  // 'exit', as the JavaScript code may be what keeps the process running.
  RunAtExit(env);
  env->Exit(ExitCode::kProcessTimeout);
}

SigintWatchdog::SigintWatchdog(
  v8::Isolate* isolate, bool* received_signal)
    : isolate_(isolate), received_signal_(received_signal) {
  Mutex::ScopedLock lock(SigintWatchdogHelper::GetInstanceActionMutex());
  // Register this watchdog with the global SIGINT/Ctrl+C listener.
  SigintWatchdogHelper::GetInstance()->Register(this);
  // Start the helper thread, if that has not already happened.
  SigintWatchdogHelper::GetInstance()->Start();
}


SigintWatchdog::~SigintWatchdog() {
  Mutex::ScopedLock lock(SigintWatchdogHelper::GetInstanceActionMutex());
  SigintWatchdogHelper::GetInstance()->Unregister(this);
  SigintWatchdogHelper::GetInstance()->Stop();
}

SignalPropagation SigintWatchdog::HandleSigint() {
  *received_signal_ = true;
  isolate_->TerminateExecution();
  return SignalPropagation::kStopPropagation;
}

void TraceSigintWatchdog::Init(Environment* env, Local<Object> target) {
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> constructor = NewFunctionTemplate(isolate, New);
  constructor->InstanceTemplate()->SetInternalFieldCount(
      TraceSigintWatchdog::kInternalFieldCount);
  constructor->Inherit(HandleWrap::GetConstructorTemplate(env));

  SetProtoMethod(isolate, constructor, "start", Start);
  SetProtoMethod(isolate, constructor, "stop", Stop);

  SetConstructorFunction(
      env->context(), target, "TraceSigintWatchdog", constructor);
}

void TraceSigintWatchdog::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new TraceSigintWatchdog(env, args.This());
}

void TraceSigintWatchdog::Start(const FunctionCallbackInfo<Value>& args) {
  TraceSigintWatchdog* watchdog;
  ASSIGN_OR_RETURN_UNWRAP(&watchdog, args.This());
  Mutex::ScopedLock lock(SigintWatchdogHelper::GetInstanceActionMutex());
  // Register this watchdog with the global SIGINT/Ctrl+C listener.
  SigintWatchdogHelper::GetInstance()->Register(watchdog);
  // Start the helper thread, if that has not already happened.
  int r = SigintWatchdogHelper::GetInstance()->Start();
  CHECK_EQ(r, 0);
}

void TraceSigintWatchdog::Stop(const FunctionCallbackInfo<Value>& args) {
  TraceSigintWatchdog* watchdog;
  ASSIGN_OR_RETURN_UNWRAP(&watchdog, args.This());
  Mutex::ScopedLock lock(SigintWatchdogHelper::GetInstanceActionMutex());
  SigintWatchdogHelper::GetInstance()->Unregister(watchdog);
  SigintWatchdogHelper::GetInstance()->Stop();
}

TraceSigintWatchdog::TraceSigintWatchdog(Environment* env, Local<Object> object)
    : HandleWrap(env,
                 object,
                 reinterpret_cast<uv_handle_t*>(&handle_),
                 AsyncWrap::PROVIDER_SIGINTWATCHDOG) {
  int r = uv_async_init(env->event_loop(), &handle_, [](uv_async_t* handle) {
    TraceSigintWatchdog* watchdog =
        ContainerOf(&TraceSigintWatchdog::handle_, handle);
    watchdog->signal_flag_ = SignalFlags::FromIdle;
    watchdog->HandleInterrupt();
  });
  CHECK_EQ(r, 0);
  uv_unref(reinterpret_cast<uv_handle_t*>(&handle_));
}

SignalPropagation TraceSigintWatchdog::HandleSigint() {
  /**
   * In case of uv loop polling, i.e. no JS currently running, activate the
   * loop to run a piece of JS code to trigger interruption.
   */
  CHECK_EQ(uv_async_send(&handle_), 0);
  env()->isolate()->RequestInterrupt(
      [](v8::Isolate* isolate, void* data) {
        TraceSigintWatchdog* self = static_cast<TraceSigintWatchdog*>(data);
        if (self->signal_flag_ == SignalFlags::None) {
          self->signal_flag_ = SignalFlags::FromInterrupt;
        }
        self->HandleInterrupt();
      },
      this);
  return SignalPropagation::kContinuePropagation;
}

void TraceSigintWatchdog::HandleInterrupt() {
  // Do not nest interrupts.
  if (interrupting) {
    return;
  }
  interrupting = true;
  if (signal_flag_ == SignalFlags::None) {
    return;
  }
  Environment* env_ = env();
  // FIXME: Before
  // https://github.com/nodejs/node/pull/29207#issuecomment-527667993 get
  // fixed, additional JavaScript code evaluation shall be prevented from
  // running during interruption.
  FPrintF(stderr,
      "KEYBOARD_INTERRUPT: Script execution was interrupted by `SIGINT`\n");
  if (signal_flag_ == SignalFlags::FromInterrupt) {
    PrintStackTrace(env_->isolate(),
                    v8::StackTrace::CurrentStackTrace(
                        env_->isolate(), 10, v8::StackTrace::kDetailed));
  }
  signal_flag_ = SignalFlags::None;
  interrupting = false;

  Mutex::ScopedLock lock(SigintWatchdogHelper::GetInstanceActionMutex());
  SigintWatchdogHelper::GetInstance()->Unregister(this);
  SigintWatchdogHelper::GetInstance()->Stop();
  raise(SIGINT);
}

#ifdef __POSIX__
void* SigintWatchdogHelper::RunSigintWatchdog(void* arg) {
  uv_thread_setname("SigintWatchdog");
  // Inside the helper thread.
  bool is_stopping;
  do {
    uv_sem_wait(&instance.sem_);
    is_stopping = InformWatchdogsAboutSignal();
  } while (!is_stopping);

  return nullptr;
}

void SigintWatchdogHelper::HandleSignal(int signum,
                                        siginfo_t* info,
                                        void* ucontext) {
  uv_sem_post(&instance.sem_);
}

#else

// Windows starts a separate thread for executing the handler, so no extra
// helper thread is required.
BOOL WINAPI SigintWatchdogHelper::WinCtrlCHandlerRoutine(DWORD dwCtrlType) {
  if (!instance.watchdog_disabled_ &&
      (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)) {
    InformWatchdogsAboutSignal();

    // Return true because the signal has been handled.
    return TRUE;
  } else {
    return FALSE;
  }
}
#endif


bool SigintWatchdogHelper::InformWatchdogsAboutSignal() {
  Mutex::ScopedLock list_lock(instance.list_mutex_);

  bool is_stopping = false;
#ifdef __POSIX__
  is_stopping = instance.stopping_;
#endif

  // If there are no listeners and the helper thread has been awoken by a signal
  // (= not when stopping it), indicate that by setting has_pending_signal_.
  if (instance.watchdogs_.empty() && !is_stopping) {
    instance.has_pending_signal_ = true;
  }

  for (auto it = instance.watchdogs_.rbegin(); it != instance.watchdogs_.rend();
       it++) {
    SignalPropagation wp = (*it)->HandleSigint();
    if (wp == SignalPropagation::kStopPropagation) {
      break;
    }
  }

  return is_stopping;
}


int SigintWatchdogHelper::Start() {
  Mutex::ScopedLock lock(mutex_);

  if (start_stop_count_++ > 0) {
    return 0;
  }

#ifdef __POSIX__
  CHECK_EQ(has_running_thread_, false);
  has_pending_signal_ = false;
  stopping_ = false;

  sigset_t sigmask;
  sigfillset(&sigmask);
  sigset_t savemask;
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &savemask));
  sigmask = savemask;
  int ret = pthread_create(&thread_, nullptr, RunSigintWatchdog, nullptr);

  auto cleanup = OnScopeLeave(
      [&]() { CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr)); });

  if (ret != 0) {
    return ret;
  }
  has_running_thread_ = true;

  RegisterSignalHandler(SIGINT, HandleSignal);
#else
  if (watchdog_disabled_) {
    watchdog_disabled_ = false;
  } else {
    SetConsoleCtrlHandler(WinCtrlCHandlerRoutine, TRUE);
  }
#endif

  return 0;
}


bool SigintWatchdogHelper::Stop() {
  bool had_pending_signal;
  Mutex::ScopedLock lock(mutex_);

  {
    Mutex::ScopedLock list_lock(list_mutex_);

    had_pending_signal = has_pending_signal_;

    if (--start_stop_count_ > 0) {
      has_pending_signal_ = false;
      return had_pending_signal;
    }

#ifdef __POSIX__
    // Set stopping now because it's only protected by list_mutex_.
    stopping_ = true;
#endif

    watchdogs_.clear();
  }

#ifdef __POSIX__
  if (!has_running_thread_) {
    has_pending_signal_ = false;
    return had_pending_signal;
  }

  // Wake up the helper thread.
  uv_sem_post(&sem_);

  // Wait for the helper thread to finish.
  CHECK_EQ(0, pthread_join(thread_, nullptr));
  has_running_thread_ = false;

  RegisterSignalHandler(SIGINT, SignalExit, true);
#else
  watchdog_disabled_ = true;
#endif

  had_pending_signal = has_pending_signal_;
  has_pending_signal_ = false;

  return had_pending_signal;
}


bool SigintWatchdogHelper::HasPendingSignal() {
  Mutex::ScopedLock lock(list_mutex_);

  return has_pending_signal_;
}

void SigintWatchdogHelper::Register(SigintWatchdogBase* wd) {
  Mutex::ScopedLock lock(list_mutex_);

  watchdogs_.push_back(wd);
}

void SigintWatchdogHelper::Unregister(SigintWatchdogBase* wd) {
  Mutex::ScopedLock lock(list_mutex_);

  auto it = std::ranges::find(watchdogs_, wd);

  CHECK_NE(it, watchdogs_.end());
  watchdogs_.erase(it);
}


SigintWatchdogHelper::SigintWatchdogHelper()
    : start_stop_count_(0),
      has_pending_signal_(false) {
#ifdef __POSIX__
  has_running_thread_ = false;
  stopping_ = false;
  CHECK_EQ(0, uv_sem_init(&sem_, 0));
#else
  watchdog_disabled_ = false;
#endif
}


SigintWatchdogHelper::~SigintWatchdogHelper() {
  start_stop_count_ = 0;
  Stop();

#ifdef __POSIX__
  CHECK_EQ(has_running_thread_, false);
  uv_sem_destroy(&sem_);
#endif
}

SigintWatchdogHelper SigintWatchdogHelper::instance;
Mutex SigintWatchdogHelper::instance_action_mutex_;

namespace watchdog {
static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  TraceSigintWatchdog::Init(env, target);
}
}  // namespace watchdog

}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(watchdog, node::watchdog::Initialize)

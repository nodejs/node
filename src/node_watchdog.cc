#include "node_watchdog.h"
#include "node_internals.h"
#include "util.h"
#include "util-inl.h"
#include <algorithm>

namespace node {

Watchdog::Watchdog(v8::Isolate* isolate, uint64_t ms) : isolate_(isolate),
                                                        timed_out_(false),
                                                        destroyed_(false) {
  int rc;
  loop_ = new uv_loop_t;
  CHECK(loop_);
  rc = uv_loop_init(loop_);
  if (rc != 0) {
    FatalError("node::Watchdog::Watchdog()",
               "Failed to initialize uv loop.");
  }

  rc = uv_async_init(loop_, &async_, &Watchdog::Async);
  CHECK_EQ(0, rc);

  rc = uv_timer_init(loop_, &timer_);
  CHECK_EQ(0, rc);

  rc = uv_timer_start(&timer_, &Watchdog::Timer, ms, 0);
  CHECK_EQ(0, rc);

  rc = uv_thread_create(&thread_, &Watchdog::Run, this);
  CHECK_EQ(0, rc);
}


Watchdog::~Watchdog() {
  Destroy();
}


void Watchdog::Dispose() {
  Destroy();
}


void Watchdog::Destroy() {
  if (destroyed_) {
    return;
  }

  uv_async_send(&async_);
  uv_thread_join(&thread_);

  uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);

  // UV_RUN_DEFAULT so that libuv has a chance to clean up.
  uv_run(loop_, UV_RUN_DEFAULT);

  int rc = uv_loop_close(loop_);
  CHECK_EQ(0, rc);
  delete loop_;
  loop_ = nullptr;

  destroyed_ = true;
}


void Watchdog::Run(void* arg) {
  Watchdog* wd = static_cast<Watchdog*>(arg);

  // UV_RUN_DEFAULT the loop will be stopped either by the async or the
  // timer handle.
  uv_run(wd->loop_, UV_RUN_DEFAULT);

  // Loop ref count reaches zero when both handles are closed.
  // Close the timer handle on this side and let Destroy() close async_
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->timer_), nullptr);
}


void Watchdog::Async(uv_async_t* async) {
  Watchdog* w = ContainerOf(&Watchdog::async_, async);
  uv_stop(w->loop_);
}


void Watchdog::Timer(uv_timer_t* timer) {
  Watchdog* w = ContainerOf(&Watchdog::timer_, timer);
  w->timed_out_ = true;
  uv_stop(w->loop_);
  w->isolate()->TerminateExecution();
}


SigintWatchdog::~SigintWatchdog() {
  Destroy();
}


void SigintWatchdog::Dispose() {
  Destroy();
}


SigintWatchdog::SigintWatchdog(v8::Isolate* isolate)
    : isolate_(isolate), received_signal_(false), destroyed_(false) {
  // Register this watchdog with the global SIGINT/Ctrl+C listener.
  SigintWatchdogHelper::GetInstance()->Register(this);
  // Start the helper thread, if that has not already happened.
  SigintWatchdogHelper::GetInstance()->Start();
}


void SigintWatchdog::Destroy() {
  if (destroyed_) {
    return;
  }

  destroyed_ = true;

  SigintWatchdogHelper::GetInstance()->Unregister(this);
  SigintWatchdogHelper::GetInstance()->Stop();
}


void SigintWatchdog::HandleSigint() {
  received_signal_ = true;
  isolate_->TerminateExecution();
}

#ifdef __POSIX__
void* SigintWatchdogHelper::RunSigintWatchdog(void* arg) {
  // Inside the helper thread.
  bool is_stopping;

  do {
    uv_sem_wait(&instance.sem_);
    is_stopping = InformWatchdogsAboutSignal();
  } while (!is_stopping);

  return nullptr;
}


void SigintWatchdogHelper::HandleSignal(int signum) {
  uv_sem_post(&instance.sem_);
}

#else

// Windows starts a separate thread for executing the handler, so no extra
// helper thread is required.
BOOL WINAPI SigintWatchdogHelper::WinCtrlCHandlerRoutine(DWORD dwCtrlType) {
  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
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

  for (auto it : instance.watchdogs_)
    it->HandleSigint();

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
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &sigmask));
  int ret = pthread_create(&thread_, nullptr, RunSigintWatchdog, nullptr);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  if (ret != 0) {
    return ret;
  }
  has_running_thread_ = true;

  RegisterSignalHandler(SIGINT, HandleSignal);
#else
  SetConsoleCtrlHandler(WinCtrlCHandlerRoutine, TRUE);
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
  SetConsoleCtrlHandler(WinCtrlCHandlerRoutine, FALSE);
#endif

  had_pending_signal = has_pending_signal_;
  has_pending_signal_ = false;

  return had_pending_signal;
}


bool SigintWatchdogHelper::HasPendingSignal() {
  Mutex::ScopedLock lock(list_mutex_);

  return has_pending_signal_;
}


void SigintWatchdogHelper::Register(SigintWatchdog* wd) {
  Mutex::ScopedLock lock(list_mutex_);

  watchdogs_.push_back(wd);
}


void SigintWatchdogHelper::Unregister(SigintWatchdog* wd) {
  Mutex::ScopedLock lock(list_mutex_);

  auto it = std::find(watchdogs_.begin(), watchdogs_.end(), wd);

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

}  // namespace node

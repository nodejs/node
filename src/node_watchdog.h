#ifndef SRC_NODE_WATCHDOG_H_
#define SRC_NODE_WATCHDOG_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include "uv.h"
#include "node_mutex.h"
#include <vector>

#ifdef __POSIX__
#include <pthread.h>
#endif

namespace node {

class Watchdog {
 public:
  explicit Watchdog(v8::Isolate* isolate, uint64_t ms);
  ~Watchdog();

  void Dispose();

  v8::Isolate* isolate() { return isolate_; }
  bool HasTimedOut() { return timed_out_; }
 private:
  void Destroy();

  static void Run(void* arg);
  static void Async(uv_async_t* async);
  static void Timer(uv_timer_t* timer);

  v8::Isolate* isolate_;
  uv_thread_t thread_;
  uv_loop_t* loop_;
  uv_async_t async_;
  uv_timer_t timer_;
  bool timed_out_;
  bool destroyed_;
};

class SigintWatchdog {
 public:
  explicit SigintWatchdog(v8::Isolate* isolate);
  ~SigintWatchdog();

  void Dispose();

  v8::Isolate* isolate() { return isolate_; }
  bool HasReceivedSignal() { return received_signal_; }
  void HandleSigint();

 private:
  void Destroy();

  v8::Isolate* isolate_;
  bool received_signal_;
  bool destroyed_;
};

class SigintWatchdogHelper {
 public:
  static SigintWatchdogHelper* GetInstance() { return &instance; }
  void Register(SigintWatchdog* watchdog);
  void Unregister(SigintWatchdog* watchdog);
  bool HasPendingSignal();

  int Start();
  bool Stop();

 private:
  SigintWatchdogHelper();
  ~SigintWatchdogHelper();

  static bool InformWatchdogsAboutSignal();
  static SigintWatchdogHelper instance;

  int start_stop_count_;

  Mutex mutex_;
  Mutex list_mutex_;
  std::vector<SigintWatchdog*> watchdogs_;
  bool has_pending_signal_;

#ifdef __POSIX__
  pthread_t thread_;
  uv_sem_t sem_;
  bool has_running_thread_;
  bool stopping_;

  static void* RunSigintWatchdog(void* arg);
  static void HandleSignal(int signum);
#else
  static BOOL WINAPI WinCtrlCHandlerRoutine(DWORD dwCtrlType);
#endif
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WATCHDOG_H_

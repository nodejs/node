/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/public/consumer_api.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/ipc/consumer_ipc_client.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/tracing/core/trace_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

#include "protos/perfetto/config/trace_config.gen.h"

#define PERFETTO_EXPORTED_API __attribute__((visibility("default")))

namespace perfetto {
namespace consumer {

namespace {

class TracingSession : public Consumer {
 public:
  TracingSession(base::TaskRunner*,
                 Handle,
                 OnStateChangedCb,
                 void* callback_arg,
                 const TraceConfig&);
  ~TracingSession() override;

  // Note: if making this class moveable, the move-ctor/dtor must be updated
  // to clear up mapped_buf_ on dtor.

  // These methods are called on a thread != |task_runner_|.
  State state() const { return state_; }
  std::pair<char*, size_t> mapped_buf() const {
    // The comparison operator will do an acquire-load on the atomic |state_|.
    if (state_ == State::kTraceEnded)
      return std::make_pair(mapped_buf_, mapped_buf_size_);
    return std::make_pair(nullptr, 0);
  }

  // All the methods below are called only on the |task_runner_| thread.

  bool Initialize();
  void StartTracing();

  // perfetto::Consumer implementation.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled() override;
  void OnTraceData(std::vector<TracePacket>, bool has_more) override;
  void OnDetach(bool) override;
  void OnAttach(bool, const TraceConfig&) override;
  void OnTraceStats(bool, const TraceStats&) override;
  void OnObservableEvents(const ObservableEvents&) override;

 private:
  TracingSession(const TracingSession&) = delete;
  TracingSession& operator=(const TracingSession&) = delete;

  void DestroyConnection();
  void NotifyCallback();

  base::TaskRunner* const task_runner_;
  Handle const handle_;
  OnStateChangedCb const callback_ = nullptr;
  void* const callback_arg_ = nullptr;
  TraceConfig trace_config_;
  base::ScopedFile buf_fd_;
  std::unique_ptr<TracingService::ConsumerEndpoint> consumer_endpoint_;

  // |mapped_buf_| and |mapped_buf_size_| are seq-consistent with |state_|.
  std::atomic<State> state_{State::kIdle};
  char* mapped_buf_ = nullptr;
  size_t mapped_buf_size_ = 0;

  PERFETTO_THREAD_CHECKER(thread_checker_)
};

TracingSession::TracingSession(base::TaskRunner* task_runner,
                               Handle handle,
                               OnStateChangedCb callback,
                               void* callback_arg,
                               const TraceConfig& trace_config_proto)
    : task_runner_(task_runner),
      handle_(handle),
      callback_(callback),
      callback_arg_(callback_arg) {
  PERFETTO_DETACH_FROM_THREAD(thread_checker_);
  trace_config_ = trace_config_proto;
  trace_config_.set_write_into_file(true);

  // TODO(primiano): this really doesn't matter because the trace will be
  // flushed into the file when stopping. We need a way to be able to say
  // "disable periodic flushing and flush only when stopping".
  trace_config_.set_file_write_period_ms(60000);
}

TracingSession::~TracingSession() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (mapped_buf_)
    PERFETTO_CHECK(munmap(mapped_buf_, mapped_buf_size_) == 0);
}

bool TracingSession::Initialize() {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  if (state_ != State::kIdle)
    return false;

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  char memfd_name[64];
  snprintf(memfd_name, sizeof(memfd_name), "perfetto_trace_%" PRId64, handle_);
  buf_fd_.reset(
      static_cast<int>(syscall(__NR_memfd_create, memfd_name, MFD_CLOEXEC)));
#else
  // Fallback for testing on Linux/mac.
  buf_fd_ = base::TempFile::CreateUnlinked().ReleaseFD();
#endif

  if (!buf_fd_) {
    PERFETTO_PLOG("Failed to allocate temporary tracing buffer");
    return false;
  }

  state_ = State::kConnecting;
  consumer_endpoint_ =
      ConsumerIPCClient::Connect(GetConsumerSocket(), this, task_runner_);

  return true;
}

// Called after EnabledTracing, soon after the IPC connection is established.
void TracingSession::OnConnect() {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  PERFETTO_DLOG("OnConnect");
  PERFETTO_DCHECK(state_ == State::kConnecting);
  consumer_endpoint_->EnableTracing(trace_config_,
                                    base::ScopedFile(dup(*buf_fd_)));
  if (trace_config_.deferred_start())
    state_ = State::kConfigured;
  else
    state_ = State::kTracing;
  NotifyCallback();
}

void TracingSession::StartTracing() {
  PERFETTO_DCHECK_THREAD(thread_checker_);

  auto state = state_.load();
  if (state != State::kConfigured) {
    PERFETTO_ELOG("StartTracing(): invalid state (%d)",
                  static_cast<int>(state));
    return;
  }
  state_ = State::kTracing;
  consumer_endpoint_->StartTracing();
}

void TracingSession::OnTracingDisabled() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("OnTracingDisabled");

  struct stat stat_buf {};
  int res = fstat(buf_fd_.get(), &stat_buf);
  mapped_buf_size_ = res == 0 ? static_cast<size_t>(stat_buf.st_size) : 0;
  mapped_buf_ =
      static_cast<char*>(mmap(nullptr, mapped_buf_size_, PROT_READ | PROT_WRITE,
                              MAP_SHARED, buf_fd_.get(), 0));
  DestroyConnection();
  if (mapped_buf_size_ == 0 || mapped_buf_ == MAP_FAILED) {
    mapped_buf_ = nullptr;
    mapped_buf_size_ = 0;
    state_ = State::kTraceFailed;
    PERFETTO_ELOG("Tracing session failed");
  } else {
    state_ = State::kTraceEnded;
  }
  NotifyCallback();
}

void TracingSession::OnDisconnect() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("OnDisconnect");
  DestroyConnection();
  state_ = State::kConnectionError;
  NotifyCallback();
}

void TracingSession::OnDetach(bool) {
  PERFETTO_DCHECK(false);  // Should never be called, Detach() is not used here.
}

void TracingSession::OnAttach(bool, const TraceConfig&) {
  PERFETTO_DCHECK(false);  // Should never be called, Attach() is not used here.
}

void TracingSession::OnTraceStats(bool, const TraceStats&) {
  // Should never be called, GetTraceStats() is not used here.
  PERFETTO_DCHECK(false);
}

void TracingSession::OnObservableEvents(const ObservableEvents&) {
  // Should never be called, ObserveEvents() is not used here.
  PERFETTO_DCHECK(false);
}

void TracingSession::DestroyConnection() {
  // Destroys the connection in a separate task. This is to avoid destroying
  // the IPC connection directly from within the IPC callback.
  TracingService::ConsumerEndpoint* endpoint = consumer_endpoint_.release();
  task_runner_->PostTask([endpoint] { delete endpoint; });
}

void TracingSession::OnTraceData(std::vector<TracePacket>, bool) {
  // This should be never called because we are using |write_into_file| and
  // asking the traced service to directly write into the |buf_fd_|.
  PERFETTO_DFATAL("Should be unreachable.");
}

void TracingSession::NotifyCallback() {
  if (!callback_)
    return;
  auto state = state_.load();
  auto callback = callback_;
  auto handle = handle_;
  auto callback_arg = callback_arg_;
  task_runner_->PostTask([callback, callback_arg, handle, state] {
    callback(handle, state, callback_arg);
  });
}

class TracingController {
 public:
  static TracingController* GetInstance();
  TracingController();

  // These methods are called from a thread != |task_runner_|.
  Handle Create(const void*, size_t, OnStateChangedCb, void* callback_arg);
  void StartTracing(Handle);
  State PollState(Handle);
  TraceBuffer ReadTrace(Handle);
  void Destroy(Handle);

 private:
  void ThreadMain();  // Called on |task_runner_| thread.

  std::mutex mutex_;
  std::thread thread_;
  std::unique_ptr<base::UnixTaskRunner> task_runner_;
  std::condition_variable task_runner_initialized_;
  Handle last_handle_ = 0;
  std::map<Handle, std::unique_ptr<TracingSession>> sessions_;
};

TracingController* TracingController::GetInstance() {
  static TracingController* instance = new TracingController();
  return instance;
}

TracingController::TracingController()
    : thread_(&TracingController::ThreadMain, this) {
  std::unique_lock<std::mutex> lock(mutex_);
  task_runner_initialized_.wait(lock, [this] { return !!task_runner_; });
}

void TracingController::ThreadMain() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    task_runner_.reset(new base::UnixTaskRunner());
  }
  task_runner_initialized_.notify_one();
  task_runner_->Run();
}

Handle TracingController::Create(const void* config_proto_buf,
                                 size_t config_len,
                                 OnStateChangedCb callback,
                                 void* callback_arg) {
  TraceConfig config_proto;
  bool parsed = config_proto.ParseFromArray(config_proto_buf, config_len);
  if (!parsed) {
    PERFETTO_ELOG("Failed to decode TraceConfig proto");
    return kInvalidHandle;
  }

  if (!config_proto.duration_ms()) {
    PERFETTO_ELOG("The trace config must specify a duration");
    return kInvalidHandle;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  Handle handle = ++last_handle_;
  auto* session = new TracingSession(task_runner_.get(), handle, callback,
                                     callback_arg, config_proto);
  sessions_.emplace(handle, std::unique_ptr<TracingSession>(session));

  // Enable the TracingSession on its own thread.
  task_runner_->PostTask([session] { session->Initialize(); });

  return handle;
}

void TracingController::StartTracing(Handle handle) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(handle);
  if (it == sessions_.end()) {
    PERFETTO_ELOG("StartTracing(): Invalid tracing session handle");
    return;
  }
  TracingSession* session = it->second.get();
  task_runner_->PostTask([session] { session->StartTracing(); });
}

State TracingController::PollState(Handle handle) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(handle);
  if (it == sessions_.end())
    return State::kSessionNotFound;
  return it->second->state();
}

TraceBuffer TracingController::ReadTrace(Handle handle) {
  TraceBuffer buf{};

  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(handle);
  if (it == sessions_.end()) {
    PERFETTO_DLOG("Handle invalid");
    return buf;
  }

  TracingSession* session = it->second.get();
  auto state = session->state();
  if (state == State::kTraceEnded) {
    std::tie(buf.begin, buf.size) = session->mapped_buf();
    return buf;
  }

  PERFETTO_DLOG("ReadTrace(): called in an unexpected state (%d)",
                static_cast<int>(state));
  return buf;
}

void TracingController::Destroy(Handle handle) {
  // Post an empty task on the task runner to delete the session on its own
  // thread.
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = sessions_.find(handle);
  if (it == sessions_.end())
    return;
  TracingSession* session = it->second.release();
  sessions_.erase(it);
  task_runner_->PostTask([session] { delete session; });
}

}  // namespace

PERFETTO_EXPORTED_API Handle Create(const void* config_proto,
                                    size_t config_len,
                                    OnStateChangedCb callback,
                                    void* callback_arg) {
  return TracingController::GetInstance()->Create(config_proto, config_len,
                                                  callback, callback_arg);
}

PERFETTO_EXPORTED_API
void StartTracing(Handle handle) {
  return TracingController::GetInstance()->StartTracing(handle);
}

PERFETTO_EXPORTED_API State PollState(Handle handle) {
  return TracingController::GetInstance()->PollState(handle);
}

PERFETTO_EXPORTED_API TraceBuffer ReadTrace(Handle handle) {
  return TracingController::GetInstance()->ReadTrace(handle);
}

PERFETTO_EXPORTED_API void Destroy(Handle handle) {
  TracingController::GetInstance()->Destroy(handle);
}
}  // namespace consumer
}  // namespace perfetto

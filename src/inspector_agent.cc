#include "inspector_agent.h"

#include "inspector/main_thread_interface.h"
#include "inspector/node_string.h"
#include "inspector/tracing_agent.h"
#include "inspector/worker_agent.h"
#include "inspector/worker_inspector.h"
#include "inspector_io.h"
#include "node/inspector/protocol/Protocol.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_process.h"
#include "node_url.h"
#include "v8-inspector.h"
#include "v8-platform.h"

#include "libplatform/libplatform.h"

#include <string.h>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifdef __POSIX__
#include <limits.h>  // PTHREAD_STACK_MIN
#include <pthread.h>
#endif  // __POSIX__

namespace node {
namespace inspector {
namespace {

using node::FatalError;

using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Object;
using v8::String;
using v8::Task;
using v8::TaskRunner;
using v8::Value;

using v8_inspector::StringBuffer;
using v8_inspector::StringView;
using v8_inspector::V8Inspector;
using v8_inspector::V8InspectorClient;

static uv_sem_t start_io_thread_semaphore;
static uv_async_t start_io_thread_async;
// This is just an additional check to make sure start_io_thread_async
// is not accidentally re-used or used when uninitialized.
static std::atomic_bool start_io_thread_async_initialized { false };

class StartIoTask : public Task {
 public:
  explicit StartIoTask(Agent* agent) : agent(agent) {}

  void Run() override {
    agent->StartIoThread();
  }

 private:
  Agent* agent;
};

std::unique_ptr<StringBuffer> ToProtocolString(Isolate* isolate,
                                               Local<Value> value) {
  TwoByteValue buffer(isolate, value);
  return StringBuffer::create(StringView(*buffer, buffer.length()));
}

// Called on the main thread.
void StartIoThreadAsyncCallback(uv_async_t* handle) {
  static_cast<Agent*>(handle->data)->StartIoThread();
}

void StartIoInterrupt(Isolate* isolate, void* agent) {
  static_cast<Agent*>(agent)->StartIoThread();
}


#ifdef __POSIX__
static void StartIoThreadWakeup(int signo) {
  uv_sem_post(&start_io_thread_semaphore);
}

inline void* StartIoThreadMain(void* unused) {
  for (;;) {
    uv_sem_wait(&start_io_thread_semaphore);
    CHECK(start_io_thread_async_initialized);
    Agent* agent = static_cast<Agent*>(start_io_thread_async.data);
    if (agent != nullptr)
      agent->RequestIoThreadStart();
  }
  return nullptr;
}

static int StartDebugSignalHandler() {
  // Start a watchdog thread for calling v8::Debug::DebugBreak() because
  // it's not safe to call directly from the signal handler, it can
  // deadlock with the thread it interrupts.
  CHECK_EQ(0, uv_sem_init(&start_io_thread_semaphore, 0));
  pthread_attr_t attr;
  CHECK_EQ(0, pthread_attr_init(&attr));
  // Don't shrink the thread's stack on FreeBSD.  Said platform decided to
  // follow the pthreads specification to the letter rather than in spirit:
  // https://lists.freebsd.org/pipermail/freebsd-current/2014-March/048885.html
#ifndef __FreeBSD__
  CHECK_EQ(0, pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN));
#endif  // __FreeBSD__
  CHECK_EQ(0, pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  sigset_t sigmask;
  // Mask all signals.
  sigfillset(&sigmask);
  sigset_t savemask;
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &savemask));
  sigmask = savemask;
  pthread_t thread;
  const int err = pthread_create(&thread, &attr,
                                 StartIoThreadMain, nullptr);
  // Restore original mask
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  CHECK_EQ(0, pthread_attr_destroy(&attr));
  if (err != 0) {
    fprintf(stderr, "node[%u]: pthread_create: %s\n",
            uv_os_getpid(), strerror(err));
    fflush(stderr);
    // Leave SIGUSR1 blocked.  We don't install a signal handler,
    // receiving the signal would terminate the process.
    return -err;
  }
  RegisterSignalHandler(SIGUSR1, StartIoThreadWakeup);
  // Unblock SIGUSR1.  A pending SIGUSR1 signal will now be delivered.
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr));
  return 0;
}
#endif  // __POSIX__


#ifdef _WIN32
DWORD WINAPI StartIoThreadProc(void* arg) {
  CHECK(start_io_thread_async_initialized);
  Agent* agent = static_cast<Agent*>(start_io_thread_async.data);
  if (agent != nullptr)
    agent->RequestIoThreadStart();
  return 0;
}

static int GetDebugSignalHandlerMappingName(DWORD pid, wchar_t* buf,
                                            size_t buf_len) {
  return _snwprintf(buf, buf_len, L"node-debug-handler-%u", pid);
}

static int StartDebugSignalHandler() {
  wchar_t mapping_name[32];
  HANDLE mapping_handle;
  DWORD pid;
  LPTHREAD_START_ROUTINE* handler;

  pid = uv_os_getpid();

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    return -1;
  }

  mapping_handle = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                      nullptr,
                                      PAGE_READWRITE,
                                      0,
                                      sizeof *handler,
                                      mapping_name);
  if (mapping_handle == nullptr) {
    return -1;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping_handle,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    sizeof *handler));
  if (handler == nullptr) {
    CloseHandle(mapping_handle);
    return -1;
  }

  *handler = StartIoThreadProc;

  UnmapViewOfFile(static_cast<void*>(handler));

  return 0;
}
#endif  // _WIN32


const int CONTEXT_GROUP_ID = 1;

std::string GetWorkerLabel(node::Environment* env) {
  std::ostringstream result;
  result << "Worker[" << env->thread_id() << "]";
  return result.str();
}

class ChannelImpl final : public v8_inspector::V8Inspector::Channel,
                          public protocol::FrontendChannel {
 public:
  explicit ChannelImpl(Environment* env,
                       const std::unique_ptr<V8Inspector>& inspector,
                       std::shared_ptr<WorkerManager> worker_manager,
                       std::unique_ptr<InspectorSessionDelegate> delegate,
                       std::shared_ptr<MainThreadHandle> main_thread_,
                       bool prevent_shutdown)
      : delegate_(std::move(delegate)), prevent_shutdown_(prevent_shutdown) {
    session_ = inspector->connect(1, this, StringView());
    node_dispatcher_ = std::make_unique<protocol::UberDispatcher>(this);
    tracing_agent_ =
        std::make_unique<protocol::TracingAgent>(env, main_thread_);
    tracing_agent_->Wire(node_dispatcher_.get());
    worker_agent_ = std::make_unique<protocol::WorkerAgent>(worker_manager);
    worker_agent_->Wire(node_dispatcher_.get());
  }

  virtual ~ChannelImpl() {
    tracing_agent_->disable();
    tracing_agent_.reset();  // Dispose before the dispatchers
    worker_agent_->disable();
    worker_agent_.reset();  // Dispose before the dispatchers
  }

  std::string dispatchProtocolMessage(const StringView& message) {
    std::unique_ptr<protocol::DictionaryValue> parsed;
    std::string method;
    node_dispatcher_->getCommandName(
        protocol::StringUtil::StringViewToUtf8(message), &method, &parsed);
    if (v8_inspector::V8InspectorSession::canDispatchMethod(
            Utf8ToStringView(method)->string())) {
      session_->dispatchProtocolMessage(message);
    } else {
      node_dispatcher_->dispatch(std::move(parsed));
    }
    return method;
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    std::unique_ptr<StringBuffer> buffer = Utf8ToStringView(reason);
    session_->schedulePauseOnNextStatement(buffer->string(), buffer->string());
  }

  bool preventShutdown() {
    return prevent_shutdown_;
  }

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    sendMessageToFrontend(message->string());
  }

  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    sendMessageToFrontend(message->string());
  }

  void flushProtocolNotifications() override { }

  void sendMessageToFrontend(const StringView& message) {
    delegate_->SendMessageToFrontend(message);
  }

  void sendMessageToFrontend(const std::string& message) {
    sendMessageToFrontend(Utf8ToStringView(message)->string());
  }

  using Serializable = protocol::Serializable;

  void sendProtocolResponse(int callId,
                            std::unique_ptr<Serializable> message) override {
    sendMessageToFrontend(message->serialize());
  }
  void sendProtocolNotification(
      std::unique_ptr<Serializable> message) override {
    sendMessageToFrontend(message->serialize());
  }

  std::unique_ptr<protocol::TracingAgent> tracing_agent_;
  std::unique_ptr<protocol::WorkerAgent> worker_agent_;
  std::unique_ptr<InspectorSessionDelegate> delegate_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
  std::unique_ptr<protocol::UberDispatcher> node_dispatcher_;
  bool prevent_shutdown_;
};

class InspectorTimer {
 public:
  InspectorTimer(uv_loop_t* loop,
                 double interval_s,
                 V8InspectorClient::TimerCallback callback,
                 void* data) : timer_(),
                               callback_(callback),
                               data_(data) {
    uv_timer_init(loop, &timer_);
    int64_t interval_ms = 1000 * interval_s;
    uv_timer_start(&timer_, OnTimer, interval_ms, interval_ms);
  }

  InspectorTimer(const InspectorTimer&) = delete;

  void Stop() {
    uv_timer_stop(&timer_);
    uv_close(reinterpret_cast<uv_handle_t*>(&timer_), TimerClosedCb);
  }

 private:
  static void OnTimer(uv_timer_t* uvtimer) {
    InspectorTimer* timer = node::ContainerOf(&InspectorTimer::timer_, uvtimer);
    timer->callback_(timer->data_);
  }

  static void TimerClosedCb(uv_handle_t* uvtimer) {
    std::unique_ptr<InspectorTimer> timer(
        node::ContainerOf(&InspectorTimer::timer_,
                          reinterpret_cast<uv_timer_t*>(uvtimer)));
    // Unique_ptr goes out of scope here and pointer is deleted.
  }

  ~InspectorTimer() {}

  uv_timer_t timer_;
  V8InspectorClient::TimerCallback callback_;
  void* data_;

  friend std::unique_ptr<InspectorTimer>::deleter_type;
};

class InspectorTimerHandle {
 public:
  InspectorTimerHandle(uv_loop_t* loop, double interval_s,
                       V8InspectorClient::TimerCallback callback, void* data) {
    timer_ = new InspectorTimer(loop, interval_s, callback, data);
  }

  InspectorTimerHandle(const InspectorTimerHandle&) = delete;

  ~InspectorTimerHandle() {
    CHECK_NOT_NULL(timer_);
    timer_->Stop();
    timer_ = nullptr;
  }
 private:
  InspectorTimer* timer_;
};

class SameThreadInspectorSession : public InspectorSession {
 public:
  SameThreadInspectorSession(
      int session_id, std::shared_ptr<NodeInspectorClient> client)
      : session_id_(session_id), client_(client) {}
  ~SameThreadInspectorSession() override;
  void Dispatch(const v8_inspector::StringView& message) override;

 private:
  int session_id_;
  std::weak_ptr<NodeInspectorClient> client_;
};

void NotifyClusterWorkersDebugEnabled(Environment* env) {
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  // Send message to enable debug in cluster workers
  Local<Object> message = Object::New(isolate);
  message->Set(context, FIXED_ONE_BYTE_STRING(isolate, "cmd"),
               FIXED_ONE_BYTE_STRING(isolate, "NODE_DEBUG_ENABLED")).FromJust();
  ProcessEmit(env, "internalMessage", message);
}

#ifdef _WIN32
bool IsFilePath(const std::string& path) {
  // '\\'
  if (path.length() > 2 && path[0] == '\\' && path[1] == '\\')
    return true;
  // '[A-Z]:[/\\]'
  if (path.length() < 3)
    return false;
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
    return path[1] == ':' && (path[2] == '/' || path[2] == '\\');
  return false;
}
#else
bool IsFilePath(const std::string& path) {
  return path.length() && path[0] == '/';
}
#endif  // __POSIX__

}  // namespace

class NodeInspectorClient : public V8InspectorClient {
 public:
  explicit NodeInspectorClient(node::Environment* env, bool is_main)
      : env_(env), is_main_(is_main) {
    client_ = V8Inspector::create(env->isolate(), this);
    // TODO(bnoordhuis) Make name configurable from src/node.cc.
    std::string name =
        is_main_ ? GetHumanReadableProcessName() : GetWorkerLabel(env);
    ContextInfo info(name);
    info.is_default = true;
    contextCreated(env->context(), info);
  }

  void runMessageLoopOnPause(int context_group_id) override {
    waiting_for_resume_ = true;
    runMessageLoop();
  }

  void waitForIoShutdown() {
    waiting_for_io_shutdown_ = true;
    runMessageLoop();
  }

  void waitForFrontend() {
    waiting_for_frontend_ = true;
    runMessageLoop();
  }

  void maxAsyncCallStackDepthChanged(int depth) override {
    if (auto agent = env_->inspector_agent()) {
      if (depth == 0) {
        agent->DisableAsyncHook();
      } else {
        agent->EnableAsyncHook();
      }
    }
  }

  void contextCreated(Local<Context> context, const ContextInfo& info) {
    auto name_buffer = Utf8ToStringView(info.name);
    auto origin_buffer = Utf8ToStringView(info.origin);
    std::unique_ptr<StringBuffer> aux_data_buffer;

    v8_inspector::V8ContextInfo v8info(
        context, CONTEXT_GROUP_ID, name_buffer->string());
    v8info.origin = origin_buffer->string();

    if (info.is_default) {
      aux_data_buffer = Utf8ToStringView("{\"isDefault\":true}");
    } else {
      aux_data_buffer = Utf8ToStringView("{\"isDefault\":false}");
    }
    v8info.auxData = aux_data_buffer->string();

    client_->contextCreated(v8info);
  }

  void contextDestroyed(Local<Context> context) {
    client_->contextDestroyed(context);
  }

  void quitMessageLoopOnPause() override {
    waiting_for_resume_ = false;
  }

  int connectFrontend(std::unique_ptr<InspectorSessionDelegate> delegate,
                      bool prevent_shutdown) {
    events_dispatched_ = true;
    int session_id = next_session_id_++;
    channels_[session_id] = std::make_unique<ChannelImpl>(env_,
                                                          client_,
                                                          getWorkerManager(),
                                                          std::move(delegate),
                                                          getThreadHandle(),
                                                          prevent_shutdown);
    return session_id;
  }

  void disconnectFrontend(int session_id) {
    events_dispatched_ = true;
    channels_.erase(session_id);
  }

  void dispatchMessageFromFrontend(int session_id, const StringView& message) {
    events_dispatched_ = true;
    std::string method =
        channels_[session_id]->dispatchProtocolMessage(message);
    if (waiting_for_frontend_)
      waiting_for_frontend_ = method != "Runtime.runIfWaitingForDebugger";
  }

  Local<Context> ensureDefaultContextInGroup(int contextGroupId) override {
    return env_->context();
  }

  void installAdditionalCommandLineAPI(Local<Context> context,
                                       Local<Object> target) override {
    Local<Function> installer = env_->inspector_console_extension_installer();
    if (!installer.IsEmpty()) {
      Local<Value> argv[] = {target};
      // If there is an exception, proceed in JS land
      USE(installer->Call(context, target, arraysize(argv), argv));
    }
  }

  void FatalException(Local<Value> error, Local<Message> message) {
    Isolate* isolate = env_->isolate();
    Local<Context> context = env_->context();

    int script_id = message->GetScriptOrigin().ScriptID()->Value();

    Local<v8::StackTrace> stack_trace = message->GetStackTrace();

    if (!stack_trace.IsEmpty() && stack_trace->GetFrameCount() > 0 &&
        script_id == stack_trace->GetFrame(isolate, 0)->GetScriptId()) {
      script_id = 0;
    }

    const uint8_t DETAILS[] = "Uncaught";

    client_->exceptionThrown(
        context,
        StringView(DETAILS, sizeof(DETAILS) - 1),
        error,
        ToProtocolString(isolate, message->Get())->string(),
        ToProtocolString(isolate, message->GetScriptResourceName())->string(),
        message->GetLineNumber(context).FromMaybe(0),
        message->GetStartColumn(context).FromMaybe(0),
        client_->createStackTrace(stack_trace),
        script_id);
  }

  void startRepeatingTimer(double interval_s,
                           TimerCallback callback,
                           void* data) override {
    timers_.emplace(std::piecewise_construct, std::make_tuple(data),
                    std::make_tuple(env_->event_loop(), interval_s, callback,
                                    data));
  }

  void cancelTimer(void* data) override {
    timers_.erase(data);
  }

  // Async stack traces instrumentation.
  void AsyncTaskScheduled(const StringView& task_name, void* task,
                          bool recurring) {
    client_->asyncTaskScheduled(task_name, task, recurring);
  }

  void AsyncTaskCanceled(void* task) {
    client_->asyncTaskCanceled(task);
  }

  void AsyncTaskStarted(void* task) {
    client_->asyncTaskStarted(task);
  }

  void AsyncTaskFinished(void* task) {
    client_->asyncTaskFinished(task);
  }

  void AllAsyncTasksCanceled() {
    client_->allAsyncTasksCanceled();
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    for (const auto& id_channel : channels_) {
      id_channel.second->schedulePauseOnNextStatement(reason);
    }
  }

  bool hasConnectedSessions() {
    for (const auto& id_channel : channels_) {
      // Other sessions are "invisible" more most purposes
      if (id_channel.second->preventShutdown())
        return true;
    }
    return false;
  }

  std::shared_ptr<MainThreadHandle> getThreadHandle() {
    if (interface_ == nullptr) {
      interface_.reset(new MainThreadInterface(
          env_->inspector_agent(), env_->event_loop(), env_->isolate(),
          env_->isolate_data()->platform()));
    }
    return interface_->GetHandle();
  }

  std::shared_ptr<WorkerManager> getWorkerManager() {
    if (worker_manager_ == nullptr) {
      worker_manager_ =
          std::make_shared<WorkerManager>(getThreadHandle());
    }
    return worker_manager_;
  }

  bool IsActive() {
    return !channels_.empty();
  }

 private:
  bool shouldRunMessageLoop() {
    if (waiting_for_frontend_)
      return true;
    if (waiting_for_io_shutdown_ || waiting_for_resume_)
      return hasConnectedSessions();
    return false;
  }

  void runMessageLoop() {
    if (running_nested_loop_)
      return;

    running_nested_loop_ = true;

    MultiIsolatePlatform* platform = env_->isolate_data()->platform();
    while (shouldRunMessageLoop()) {
      if (interface_ && hasConnectedSessions())
        interface_->WaitForFrontendEvent();
      while (platform->FlushForegroundTasks(env_->isolate())) {}
    }
    running_nested_loop_ = false;
  }

  double currentTimeMS() override {
    return env_->isolate_data()->platform()->CurrentClockTimeMillis();
  }

  std::unique_ptr<StringBuffer> resourceNameToUrl(
      const StringView& resource_name_view) override {
    std::string resource_name =
        protocol::StringUtil::StringViewToUtf8(resource_name_view);
    if (!IsFilePath(resource_name))
      return nullptr;
    node::url::URL url = node::url::URL::FromFilePath(resource_name);
    // TODO(ak239spb): replace this code with url.href().
    // Refs: https://github.com/nodejs/node/issues/22610
    return Utf8ToStringView(url.protocol() + "//" + url.path());
  }

  node::Environment* env_;
  bool is_main_;
  bool running_nested_loop_ = false;
  std::unique_ptr<V8Inspector> client_;
  std::unordered_map<int, std::unique_ptr<ChannelImpl>> channels_;
  std::unordered_map<void*, InspectorTimerHandle> timers_;
  int next_session_id_ = 1;
  bool events_dispatched_ = false;
  bool waiting_for_resume_ = false;
  bool waiting_for_frontend_ = false;
  bool waiting_for_io_shutdown_ = false;
  // Allows accessing Inspector from non-main threads
  std::unique_ptr<MainThreadInterface> interface_;
  std::shared_ptr<WorkerManager> worker_manager_;
};

Agent::Agent(Environment* env)
    : parent_env_(env),
      debug_options_(env->options()->debug_options()),
      host_port_(env->inspector_host_port()) {}

Agent::~Agent() {
  if (start_io_thread_async.data == this) {
    CHECK(start_io_thread_async_initialized.exchange(false));
    start_io_thread_async.data = nullptr;
    // This is global, will never get freed
    uv_close(reinterpret_cast<uv_handle_t*>(&start_io_thread_async), nullptr);
  }
}

bool Agent::Start(const std::string& path,
                  const DebugOptions& options,
                  std::shared_ptr<HostPort> host_port,
                  bool is_main) {
  path_ = path;
  debug_options_ = options;
  CHECK_NOT_NULL(host_port);
  host_port_ = host_port;

  client_ = std::make_shared<NodeInspectorClient>(parent_env_, is_main);
  if (parent_env_->owns_inspector()) {
    CHECK_EQ(start_io_thread_async_initialized.exchange(true), false);
    CHECK_EQ(0, uv_async_init(parent_env_->event_loop(),
                              &start_io_thread_async,
                              StartIoThreadAsyncCallback));
    uv_unref(reinterpret_cast<uv_handle_t*>(&start_io_thread_async));
    start_io_thread_async.data = this;
    // Ignore failure, SIGUSR1 won't work, but that should not block node start.
    StartDebugSignalHandler();
  }

  bool wait_for_connect = options.wait_for_connect();
  if (parent_handle_) {
    wait_for_connect = parent_handle_->WaitForConnect();
    parent_handle_->WorkerStarted(client_->getThreadHandle(), wait_for_connect);
  } else if (!options.inspector_enabled || !StartIoThread()) {
    return false;
  }

  // TODO(joyeecheung): we should not be using process as a global object
  // to transport --inspect-brk. Instead, the JS land can get this through
  // require('internal/options') since it should be set once CLI parsing
  // is done.
  if (wait_for_connect) {
    HandleScope scope(parent_env_->isolate());
    parent_env_->process_object()->DefineOwnProperty(
        parent_env_->context(),
        FIXED_ONE_BYTE_STRING(parent_env_->isolate(), "_breakFirstLine"),
        True(parent_env_->isolate()),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum))
        .FromJust();
    client_->waitForFrontend();
  }
  return true;
}

bool Agent::StartIoThread() {
  if (io_ != nullptr)
    return true;

  CHECK_NOT_NULL(client_);

  io_ = InspectorIo::Start(client_->getThreadHandle(), path_, host_port_);
  if (io_ == nullptr) {
    return false;
  }
  NotifyClusterWorkersDebugEnabled(parent_env_);
  return true;
}

void Agent::Stop() {
  io_.reset();
}

std::unique_ptr<InspectorSession> Agent::Connect(
    std::unique_ptr<InspectorSessionDelegate> delegate,
    bool prevent_shutdown) {
  CHECK_NOT_NULL(client_);
  int session_id = client_->connectFrontend(std::move(delegate),
                                            prevent_shutdown);
  return std::unique_ptr<InspectorSession>(
      new SameThreadInspectorSession(session_id, client_));
}

void Agent::WaitForDisconnect() {
  CHECK_NOT_NULL(client_);
  bool is_worker = parent_handle_ != nullptr;
  parent_handle_.reset();
  if (client_->hasConnectedSessions() && !is_worker) {
    fprintf(stderr, "Waiting for the debugger to disconnect...\n");
    fflush(stderr);
  }
  // TODO(addaleax): Maybe this should use an at-exit hook for the Environment
  // or something similar?
  client_->contextDestroyed(parent_env_->context());
  if (io_ != nullptr) {
    io_->StopAcceptingNewConnections();
    client_->waitForIoShutdown();
  }
}

void Agent::FatalException(Local<Value> error, Local<Message> message) {
  if (!IsListening())
    return;
  client_->FatalException(error, message);
  WaitForDisconnect();
}

void Agent::PauseOnNextJavascriptStatement(const std::string& reason) {
  client_->schedulePauseOnNextStatement(reason);
}

void Agent::RegisterAsyncHook(Isolate* isolate,
                              Local<Function> enable_function,
                              Local<Function> disable_function) {
  enable_async_hook_function_.Reset(isolate, enable_function);
  disable_async_hook_function_.Reset(isolate, disable_function);
  if (pending_enable_async_hook_) {
    CHECK(!pending_disable_async_hook_);
    pending_enable_async_hook_ = false;
    EnableAsyncHook();
  } else if (pending_disable_async_hook_) {
    CHECK(!pending_enable_async_hook_);
    pending_disable_async_hook_ = false;
    DisableAsyncHook();
  }
}

void Agent::EnableAsyncHook() {
  if (!enable_async_hook_function_.IsEmpty()) {
    ToggleAsyncHook(parent_env_->isolate(), enable_async_hook_function_);
  } else if (pending_disable_async_hook_) {
    CHECK(!pending_enable_async_hook_);
    pending_disable_async_hook_ = false;
  } else {
    pending_enable_async_hook_ = true;
  }
}

void Agent::DisableAsyncHook() {
  if (!disable_async_hook_function_.IsEmpty()) {
    ToggleAsyncHook(parent_env_->isolate(), disable_async_hook_function_);
  } else if (pending_enable_async_hook_) {
    CHECK(!pending_disable_async_hook_);
    pending_enable_async_hook_ = false;
  } else {
    pending_disable_async_hook_ = true;
  }
}

void Agent::ToggleAsyncHook(Isolate* isolate,
                            const node::Persistent<Function>& fn) {
  HandleScope handle_scope(isolate);
  CHECK(!fn.IsEmpty());
  auto context = parent_env_->context();
  auto result = fn.Get(isolate)->Call(context, Undefined(isolate), 0, nullptr);
  if (result.IsEmpty()) {
    FatalError(
        "node::inspector::Agent::ToggleAsyncHook",
        "Cannot toggle Inspector's AsyncHook, please report this.");
  }
}

void Agent::AsyncTaskScheduled(const StringView& task_name, void* task,
                               bool recurring) {
  client_->AsyncTaskScheduled(task_name, task, recurring);
}

void Agent::AsyncTaskCanceled(void* task) {
  client_->AsyncTaskCanceled(task);
}

void Agent::AsyncTaskStarted(void* task) {
  client_->AsyncTaskStarted(task);
}

void Agent::AsyncTaskFinished(void* task) {
  client_->AsyncTaskFinished(task);
}

void Agent::AllAsyncTasksCanceled() {
  client_->AllAsyncTasksCanceled();
}

void Agent::RequestIoThreadStart() {
  // We need to attempt to interrupt V8 flow (in case Node is running
  // continuous JS code) and to wake up libuv thread (in case Node is waiting
  // for IO events)
  CHECK(start_io_thread_async_initialized);
  uv_async_send(&start_io_thread_async);
  Isolate* isolate = parent_env_->isolate();
  v8::Platform* platform = parent_env_->isolate_data()->platform();
  std::shared_ptr<TaskRunner> taskrunner =
    platform->GetForegroundTaskRunner(isolate);
  taskrunner->PostTask(std::make_unique<StartIoTask>(this));
  isolate->RequestInterrupt(StartIoInterrupt, this);
  CHECK(start_io_thread_async_initialized);
  uv_async_send(&start_io_thread_async);
}

void Agent::ContextCreated(Local<Context> context, const ContextInfo& info) {
  if (client_ == nullptr)  // This happens for a main context
    return;
  client_->contextCreated(context, info);
}

bool Agent::WillWaitForConnect() {
  if (debug_options_.wait_for_connect()) return true;
  if (parent_handle_)
    return parent_handle_->WaitForConnect();
  return false;
}

bool Agent::IsActive() {
  if (client_ == nullptr)
    return false;
  return io_ != nullptr || client_->IsActive();
}

void Agent::SetParentHandle(
    std::unique_ptr<ParentInspectorHandle> parent_handle) {
  parent_handle_ = std::move(parent_handle);
}

std::unique_ptr<ParentInspectorHandle> Agent::GetParentHandle(
    int thread_id, const std::string& url) {
  return client_->getWorkerManager()->NewParentHandle(thread_id, url);
}

void Agent::WaitForConnect() {
  CHECK_NOT_NULL(client_);
  client_->waitForFrontend();
}

std::shared_ptr<WorkerManager> Agent::GetWorkerManager() {
  CHECK_NOT_NULL(client_);
  return client_->getWorkerManager();
}

SameThreadInspectorSession::~SameThreadInspectorSession() {
  auto client = client_.lock();
  if (client)
    client->disconnectFrontend(session_id_);
}

void SameThreadInspectorSession::Dispatch(
    const v8_inspector::StringView& message) {
  auto client = client_.lock();
  if (client)
    client->dispatchMessageFromFrontend(session_id_, message);
}



}  // namespace inspector
}  // namespace node

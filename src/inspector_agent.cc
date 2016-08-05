#include "inspector_agent.h"

#include "inspector_socket.h"
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "node_mutex.h"
#include "node_version.h"
#include "v8-platform.h"
#include "util.h"

#include "platform/v8_inspector/public/InspectorVersion.h"
#include "platform/v8_inspector/public/V8Inspector.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"

#include "libplatform/libplatform.h"

#include <string.h>
#include <utility>
#include <vector>

// We need pid to use as ID with Chrome
#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#define getpid GetCurrentProcessId
#else
#include <unistd.h>  // setuid, getuid
#endif

namespace node {
namespace {

const char TAG_CONNECT[] = "#connect";
const char TAG_DISCONNECT[] = "#disconnect";

const char DEVTOOLS_PATH[] = "/node";
const char DEVTOOLS_HASH[] = V8_INSPECTOR_REVISION;

void PrintDebuggerReadyMessage(int port) {
  fprintf(stderr, "Debugger listening on port %d.\n"
    "Warning: This is an experimental feature and could change at any time.\n"
    "To start debugging, open the following URL in Chrome:\n"
    "    chrome-devtools://devtools/remote/serve_file/"
    "@%s/inspector.html?"
    "experiments=true&v8only=true&ws=localhost:%d/node\n",
      port, DEVTOOLS_HASH, port);
}

bool AcceptsConnection(inspector_socket_t* socket, const std::string& path) {
  return 0 == path.compare(0, sizeof(DEVTOOLS_PATH) - 1, DEVTOOLS_PATH);
}

void DisposeInspector(inspector_socket_t* socket, int status) {
  delete socket;
}

void DisconnectAndDisposeIO(inspector_socket_t* socket) {
  if (socket) {
    inspector_close(socket, DisposeInspector);
  }
}

void OnBufferAlloc(uv_handle_t* handle, size_t len, uv_buf_t* buf) {
  buf->base = new char[len];
  buf->len = len;
}

void SendHttpResponse(inspector_socket_t* socket, const char* response,
                      size_t len) {
  const char HEADERS[] = "HTTP/1.0 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Content-Length: %ld\r\n"
                         "\r\n";
  char header[sizeof(HEADERS) + 20];
  int header_len = snprintf(header, sizeof(header), HEADERS, len);
  inspector_write(socket, header, header_len);
  inspector_write(socket, response, len);
}

void SendVersionResponse(inspector_socket_t* socket) {
  const char VERSION_RESPONSE_TEMPLATE[] =
      "[ {"
      "  \"Browser\": \"node.js/%s\","
      "  \"Protocol-Version\": \"1.1\","
      "  \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
            "(KHTML, like Gecko) Chrome/45.0.2446.0 Safari/537.36\","
      "  \"WebKit-Version\": \"537.36 (@198122)\""
      "} ]";
  char buffer[sizeof(VERSION_RESPONSE_TEMPLATE) + 128];
  size_t len = snprintf(buffer, sizeof(buffer), VERSION_RESPONSE_TEMPLATE,
                        NODE_VERSION);
  ASSERT_LT(len, sizeof(buffer));
  SendHttpResponse(socket, buffer, len);
}

void SendTargentsListResponse(inspector_socket_t* socket, int port) {
  const char LIST_RESPONSE_TEMPLATE[] =
      "[ {"
      "  \"description\": \"node.js instance\","
      "  \"devtoolsFrontendUrl\": "
            "\"https://chrome-devtools-frontend.appspot.com/serve_file/"
            "@%s/inspector.html?experiments=true&v8only=true"
            "&ws=localhost:%d%s\","
      "  \"faviconUrl\": \"https://nodejs.org/static/favicon.ico\","
      "  \"id\": \"%d\","
      "  \"title\": \"%s\","
      "  \"type\": \"node\","
      "  \"webSocketDebuggerUrl\": \"ws://localhost:%d%s\""
      "} ]";
  char buffer[sizeof(LIST_RESPONSE_TEMPLATE) + 4096];
  char title[2048];  // uv_get_process_title trims the title if too long
  int err = uv_get_process_title(title, sizeof(title));
  if (err != 0) {
    snprintf(title, sizeof(title), "Node.js");
  }
  char* c = title;
  while (*c != '\0') {
    if (*c < ' ' || *c == '\"') {
      *c = '_';
    }
    c++;
  }
  size_t len = snprintf(buffer, sizeof(buffer), LIST_RESPONSE_TEMPLATE,
                        DEVTOOLS_HASH, port, DEVTOOLS_PATH, getpid(),
                        title, port, DEVTOOLS_PATH);
  ASSERT_LT(len, sizeof(buffer));
  SendHttpResponse(socket, buffer, len);
}

bool RespondToGet(inspector_socket_t* socket, const std::string& path,
                  int port) {
  const char PATH[] = "/json";
  const char PATH_LIST[] = "/json/list";
  const char PATH_VERSION[] = "/json/version";
  const char PATH_ACTIVATE[] = "/json/activate/";
  if (0 == path.compare(0, sizeof(PATH_VERSION) - 1, PATH_VERSION)) {
    SendVersionResponse(socket);
  } else if (0 == path.compare(0, sizeof(PATH_LIST) - 1, PATH_LIST) ||
             0 == path.compare(0, sizeof(PATH) - 1, PATH)) {
    SendTargentsListResponse(socket, port);
  } else if (0 == path.compare(0, sizeof(PATH_ACTIVATE) - 1, PATH_ACTIVATE) &&
             atoi(path.substr(sizeof(PATH_ACTIVATE) - 1).c_str()) == getpid()) {
    const char TARGET_ACTIVATED[] = "Target activated";
    SendHttpResponse(socket, TARGET_ACTIVATED, sizeof(TARGET_ACTIVATED) - 1);
  } else {
    return false;
  }
  return true;
}

}  // namespace

namespace inspector {


class V8NodeInspector;

class AgentImpl {
 public:
  explicit AgentImpl(node::Environment* env);
  ~AgentImpl();

  // Start the inspector agent thread
  bool Start(v8::Platform* platform, int port, bool wait);
  // Stop the inspector agent
  void Stop();

  bool IsStarted();
  bool IsConnected() {  return state_ == State::kConnected; }
  void WaitForDisconnect();

 private:
  using MessageQueue = std::vector<std::pair<int, String16>>;
  enum class State { kNew, kAccepting, kConnected, kDone, kError };

  static void ThreadCbIO(void* agent);
  static void OnSocketConnectionIO(uv_stream_t* server, int status);
  static bool OnInspectorHandshakeIO(inspector_socket_t* socket,
                                     enum inspector_handshake_event state,
                                     const std::string& path);
  static void WriteCbIO(uv_async_t* async);

  void InstallInspectorOnProcess();

  void WorkerRunIO();
  void OnInspectorConnectionIO(inspector_socket_t* socket);
  void OnRemoteDataIO(inspector_socket_t* stream, ssize_t read,
                      const uv_buf_t* b);
  void PostMessages();
  void SetConnected(bool connected);
  void DispatchMessages();
  void Write(int session_id, const String16& message);
  void AppendMessage(MessageQueue* vector, int session_id,
                     const String16& message);
  void SwapBehindLock(MessageQueue* vector1, MessageQueue* vector2);
  void PostIncomingMessage(const String16& message);
  State ToState(State state);

  uv_sem_t start_sem_;
  ConditionVariable pause_cond_;
  Mutex pause_lock_;
  Mutex queue_lock_;
  uv_thread_t thread_;
  uv_loop_t child_loop_;

  int port_;
  bool wait_;
  bool shutting_down_;
  State state_;
  node::Environment* parent_env_;

  uv_async_t* data_written_;
  uv_async_t io_thread_req_;
  inspector_socket_t* client_socket_;
  V8NodeInspector* inspector_;
  v8::Platform* platform_;
  MessageQueue incoming_message_queue_;
  MessageQueue outgoing_message_queue_;
  bool dispatching_messages_;
  int frontend_session_id_;
  int backend_session_id_;

  friend class ChannelImpl;
  friend class DispatchOnInspectorBackendTask;
  friend class SetConnectedTask;
  friend class V8NodeInspector;
  friend void InterruptCallback(v8::Isolate*, void* agent);
  friend void DataCallback(uv_stream_t* stream, ssize_t read,
                           const uv_buf_t* buf);
};

void InterruptCallback(v8::Isolate*, void* agent) {
  static_cast<AgentImpl*>(agent)->DispatchMessages();
}

void DataCallback(uv_stream_t* stream, ssize_t read, const uv_buf_t* buf) {
  inspector_socket_t* socket = static_cast<inspector_socket_t*>(stream->data);
  static_cast<AgentImpl*>(socket->data)->OnRemoteDataIO(socket, read, buf);
}

class DispatchOnInspectorBackendTask : public v8::Task {
 public:
  explicit DispatchOnInspectorBackendTask(AgentImpl* agent) : agent_(agent) {}

  void Run() override {
    agent_->DispatchMessages();
  }

 private:
  AgentImpl* agent_;
};

class ChannelImpl final : public blink::protocol::FrontendChannel {
 public:
  explicit ChannelImpl(AgentImpl* agent): agent_(agent) {}
  virtual ~ChannelImpl() {}
 private:
  void sendProtocolResponse(int callId, const String16& message) override {
    sendMessageToFrontend(message);
  }

  void sendProtocolNotification(const String16& message) override {
    sendMessageToFrontend(message);
  }

  void flushProtocolNotifications() override { }

  void sendMessageToFrontend(const String16& message) {
    agent_->Write(agent_->frontend_session_id_, message);
  }

  AgentImpl* const agent_;
};

// Used in V8NodeInspector::currentTimeMS() below.
#define NANOS_PER_MSEC 1000000

class V8NodeInspector : public blink::V8InspectorClient {
 public:
  V8NodeInspector(AgentImpl* agent, node::Environment* env,
                  v8::Platform* platform)
                  : agent_(agent),
                    isolate_(env->isolate()),
                    platform_(platform),
                    terminated_(false),
                    running_nested_loop_(false),
                    inspector_(
                        blink::V8Inspector::create(env->isolate(), this)) {
    inspector_->contextCreated(
        blink::V8ContextInfo(env->context(), 1, "NodeJS Main Context"));
  }

  void runMessageLoopOnPause(int context_group_id) override {
    if (running_nested_loop_)
      return;
    terminated_ = false;
    running_nested_loop_ = true;
    do {
      {
        Mutex::ScopedLock scoped_lock(agent_->pause_lock_);
        agent_->pause_cond_.Wait(scoped_lock);
      }
      while (v8::platform::PumpMessageLoop(platform_, isolate_))
        {}
    } while (!terminated_);
    terminated_ = false;
    running_nested_loop_ = false;
  }

  double currentTimeMS() override {
    return uv_hrtime() * 1.0 / NANOS_PER_MSEC;
  }

  void quitMessageLoopOnPause() override {
    terminated_ = true;
  }

  void connectFrontend() {
    session_ = inspector_->connect(1, new ChannelImpl(agent_), nullptr);
  }

  void disconnectFrontend() {
    session_.reset();
  }

  void dispatchMessageFromFrontend(const String16& message) {
    CHECK(session_);
    session_->dispatchProtocolMessage(message);
  }

 private:
  AgentImpl* agent_;
  v8::Isolate* isolate_;
  v8::Platform* platform_;
  bool terminated_;
  bool running_nested_loop_;
  std::unique_ptr<blink::V8Inspector> inspector_;
  std::unique_ptr<blink::V8InspectorSession> session_;
};

AgentImpl::AgentImpl(Environment* env) : port_(0),
                                         wait_(false),
                                         shutting_down_(false),
                                         state_(State::kNew),
                                         parent_env_(env),
                                         data_written_(new uv_async_t()),
                                         client_socket_(nullptr),
                                         inspector_(nullptr),
                                         platform_(nullptr),
                                         dispatching_messages_(false),
                                         frontend_session_id_(0),
                                         backend_session_id_(0) {
  CHECK_EQ(0, uv_sem_init(&start_sem_, 0));
  memset(&io_thread_req_, 0, sizeof(io_thread_req_));
  CHECK_EQ(0, uv_async_init(env->event_loop(), data_written_, nullptr));
  uv_unref(reinterpret_cast<uv_handle_t*>(data_written_));
}

AgentImpl::~AgentImpl() {
  auto close_cb = [](uv_handle_t* handle) {
    delete reinterpret_cast<uv_async_t*>(handle);
  };
  uv_close(reinterpret_cast<uv_handle_t*>(data_written_), close_cb);
  data_written_ = nullptr;
}

void InspectorConsoleCall(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  CHECK(info.Data()->IsArray());
  v8::Local<v8::Array> args = info.Data().As<v8::Array>();
  CHECK_EQ(args->Length(), 3);

  v8::Local<v8::Value> inspector_method =
      args->Get(context, 0).ToLocalChecked();
  CHECK(inspector_method->IsFunction());
  v8::Local<v8::Value> node_method =
      args->Get(context, 1).ToLocalChecked();
  CHECK(node_method->IsFunction());
  v8::Local<v8::Value> config_value =
      args->Get(context, 2).ToLocalChecked();
  CHECK(config_value->IsObject());
  v8::Local<v8::Object> config_object = config_value.As<v8::Object>();

  std::vector<v8::Local<v8::Value>> call_args(info.Length());
  for (int i = 0; i < info.Length(); ++i) {
    call_args[i] = info[i];
  }

  v8::Local<v8::String> in_call_key = OneByteString(isolate, "in_call");
  bool in_call = config_object->Has(context, in_call_key).FromMaybe(false);
  if (!in_call) {
    CHECK(config_object->Set(context,
                             in_call_key,
                             v8::True(isolate)).FromJust());
    CHECK(!inspector_method.As<v8::Function>()->Call(
        context,
        info.Holder(),
        call_args.size(),
        call_args.data()).IsEmpty());
  }

  v8::TryCatch try_catch(info.GetIsolate());
  node_method.As<v8::Function>()->Call(context,
                                       info.Holder(),
                                       call_args.size(),
                                       call_args.data());
  CHECK(config_object->Delete(context, in_call_key).FromJust());
  if (try_catch.HasCaught())
    try_catch.ReThrow();
}

void InspectorWrapConsoleCall(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 3 || !args[0]->IsFunction() ||
      !args[1]->IsFunction() || !args[2]->IsObject()) {
    return env->ThrowError("inspector.wrapConsoleCall takes exactly 3 "
        "arguments: two functions and an object.");
  }

  v8::Local<v8::Array> array = v8::Array::New(env->isolate(), args.Length());
  CHECK(array->Set(env->context(), 0, args[0]).FromJust());
  CHECK(array->Set(env->context(), 1, args[1]).FromJust());
  CHECK(array->Set(env->context(), 2, args[2]).FromJust());
  args.GetReturnValue().Set(v8::Function::New(env->context(),
                                              InspectorConsoleCall,
                                              array).ToLocalChecked());
}

bool AgentImpl::Start(v8::Platform* platform, int port, bool wait) {
  auto env = parent_env_;
  inspector_ = new V8NodeInspector(this, env, platform);
  platform_ = platform;

  InstallInspectorOnProcess();

  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);

  port_ = port;
  wait_ = wait;

  err = uv_thread_create(&thread_, AgentImpl::ThreadCbIO, this);
  CHECK_EQ(err, 0);
  uv_sem_wait(&start_sem_);

  if (state_ == State::kError) {
    Stop();
    return false;
  }
  state_ = State::kAccepting;
  if (wait) {
    DispatchMessages();
  }
  return true;
}

void AgentImpl::Stop() {
  int err = uv_thread_join(&thread_);
  CHECK_EQ(err, 0);
  delete inspector_;
}

bool AgentImpl::IsStarted() {
  return !!platform_;
}

void AgentImpl::WaitForDisconnect() {
  shutting_down_ = true;
  fprintf(stderr, "Waiting for the debugger to disconnect...\n");
  inspector_->runMessageLoopOnPause(0);
}

#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           v8::ReadOnly).FromJust();                          \
  } while (0)

void AgentImpl::InstallInspectorOnProcess() {
  auto env = parent_env_;
  v8::Local<v8::Object> process = env->process_object();
  v8::Local<v8::Object> inspector = v8::Object::New(env->isolate());
  READONLY_PROPERTY(process, "inspector", inspector);
  env->SetMethod(inspector, "wrapConsoleCall", InspectorWrapConsoleCall);
}

// static
void AgentImpl::ThreadCbIO(void* agent) {
  static_cast<AgentImpl*>(agent)->WorkerRunIO();
}

// static
void AgentImpl::OnSocketConnectionIO(uv_stream_t* server, int status) {
  if (status == 0) {
    inspector_socket_t* socket = new inspector_socket_t();
    socket->data = server->data;
    if (inspector_accept(server, socket,
                         AgentImpl::OnInspectorHandshakeIO) != 0) {
      delete socket;
    }
  }
}

// static
bool AgentImpl::OnInspectorHandshakeIO(inspector_socket_t* socket,
                                       enum inspector_handshake_event state,
                                       const std::string& path) {
  AgentImpl* agent = static_cast<AgentImpl*>(socket->data);
  switch (state) {
  case kInspectorHandshakeHttpGet:
    return RespondToGet(socket, path, agent->port_);
  case kInspectorHandshakeUpgrading:
    return AcceptsConnection(socket, path);
  case kInspectorHandshakeUpgraded:
    agent->OnInspectorConnectionIO(socket);
    return true;
  case kInspectorHandshakeFailed:
    delete socket;
    return false;
  default:
    UNREACHABLE();
  }
}

void AgentImpl::OnRemoteDataIO(inspector_socket_t* socket,
                               ssize_t read,
                               const uv_buf_t* buf) {
  Mutex::ScopedLock scoped_lock(pause_lock_);
  if (read > 0) {
    String16 str = String16::fromUTF8(buf->base, read);
    // TODO(pfeldman): Instead of blocking execution while debugger
    // engages, node should wait for the run callback from the remote client
    // and initiate its startup. This is a change to node.cc that should be
    // upstreamed separately.
    if (wait_ && str.find("\"Runtime.run\"") != std::string::npos) {
      wait_ = false;
      uv_sem_post(&start_sem_);
    }
    PostIncomingMessage(str);
  } else if (read <= 0) {
    // EOF
    if (client_socket_ == socket) {
      String16 message(TAG_DISCONNECT, sizeof(TAG_DISCONNECT) - 1);
      client_socket_ = nullptr;
      PostIncomingMessage(message);
    }
    DisconnectAndDisposeIO(socket);
  }
  if (buf) {
    delete[] buf->base;
  }
  pause_cond_.Broadcast(scoped_lock);
}

// static
void AgentImpl::WriteCbIO(uv_async_t* async) {
  AgentImpl* agent = static_cast<AgentImpl*>(async->data);
  inspector_socket_t* socket = agent->client_socket_;
  if (socket) {
    MessageQueue outgoing_messages;
    agent->SwapBehindLock(&agent->outgoing_message_queue_, &outgoing_messages);
    for (const MessageQueue::value_type& outgoing : outgoing_messages) {
      if (outgoing.first == agent->frontend_session_id_) {
        std::string message = outgoing.second.utf8();
        inspector_write(socket, message.c_str(), message.length());
      }
    }
  }
}

void AgentImpl::WorkerRunIO() {
  sockaddr_in addr;
  uv_tcp_t server;
  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);
  err = uv_async_init(&child_loop_, &io_thread_req_, AgentImpl::WriteCbIO);
  CHECK_EQ(err, 0);
  io_thread_req_.data = this;
  uv_tcp_init(&child_loop_, &server);
  uv_ip4_addr("0.0.0.0", port_, &addr);
  server.data = this;
  err = uv_tcp_bind(&server,
                    reinterpret_cast<const struct sockaddr*>(&addr), 0);
  if (err == 0) {
    err = uv_listen(reinterpret_cast<uv_stream_t*>(&server), 1,
                    OnSocketConnectionIO);
  }
  if (err != 0) {
    fprintf(stderr, "Unable to open devtools socket: %s\n", uv_strerror(err));
    state_ = State::kError;  // Safe, main thread is waiting on semaphore
    uv_close(reinterpret_cast<uv_handle_t*>(&io_thread_req_), nullptr);
    uv_close(reinterpret_cast<uv_handle_t*>(&server), nullptr);
    uv_loop_close(&child_loop_);
    uv_sem_post(&start_sem_);
    return;
  }
  PrintDebuggerReadyMessage(port_);
  if (!wait_) {
    uv_sem_post(&start_sem_);
  }
  uv_run(&child_loop_, UV_RUN_DEFAULT);
  uv_close(reinterpret_cast<uv_handle_t*>(&io_thread_req_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&server), nullptr);
  DisconnectAndDisposeIO(client_socket_);
  uv_run(&child_loop_, UV_RUN_NOWAIT);
  err = uv_loop_close(&child_loop_);
  CHECK_EQ(err, 0);
}

void AgentImpl::AppendMessage(MessageQueue* queue, int session_id,
                              const String16& message) {
  Mutex::ScopedLock scoped_lock(queue_lock_);
  queue->push_back(std::make_pair(session_id, message));
}

void AgentImpl::SwapBehindLock(MessageQueue* vector1, MessageQueue* vector2) {
  Mutex::ScopedLock scoped_lock(queue_lock_);
  vector1->swap(*vector2);
}

void AgentImpl::PostIncomingMessage(const String16& message) {
  AppendMessage(&incoming_message_queue_, frontend_session_id_, message);
  v8::Isolate* isolate = parent_env_->isolate();
  platform_->CallOnForegroundThread(isolate,
                                    new DispatchOnInspectorBackendTask(this));
  isolate->RequestInterrupt(InterruptCallback, this);
  uv_async_send(data_written_);
}

void AgentImpl::OnInspectorConnectionIO(inspector_socket_t* socket) {
  if (client_socket_) {
    DisconnectAndDisposeIO(socket);
    return;
  }
  client_socket_ = socket;
  inspector_read_start(socket, OnBufferAlloc, DataCallback);
  frontend_session_id_++;
  PostIncomingMessage(String16(TAG_CONNECT, sizeof(TAG_CONNECT) - 1));
}

void AgentImpl::DispatchMessages() {
  if (dispatching_messages_)
    return;
  dispatching_messages_ = true;
  MessageQueue tasks;
  SwapBehindLock(&incoming_message_queue_, &tasks);
  for (const MessageQueue::value_type& pair : tasks) {
    const String16& message = pair.second;
    if (message == TAG_CONNECT) {
      CHECK_EQ(State::kAccepting, state_);
      backend_session_id_++;
      state_ = State::kConnected;
      fprintf(stderr, "Debugger attached.\n");
      inspector_->connectFrontend();
    } else if (message == TAG_DISCONNECT) {
      CHECK_EQ(State::kConnected, state_);
      if (shutting_down_) {
        state_ = State::kDone;
      } else {
        PrintDebuggerReadyMessage(port_);
        state_ = State::kAccepting;
      }
      inspector_->quitMessageLoopOnPause();
      inspector_->disconnectFrontend();
    } else {
      inspector_->dispatchMessageFromFrontend(message);
    }
  }
  uv_async_send(data_written_);
  dispatching_messages_ = false;
}

void AgentImpl::Write(int session_id, const String16& message) {
  AppendMessage(&outgoing_message_queue_, session_id, message);
  int err = uv_async_send(&io_thread_req_);
  CHECK_EQ(0, err);
}

// Exported class Agent
Agent::Agent(node::Environment* env) : impl(new AgentImpl(env)) {}

Agent::~Agent() {
  delete impl;
}

bool Agent::Start(v8::Platform* platform, int port, bool wait) {
  return impl->Start(platform, port, wait);
}

void Agent::Stop() {
  impl->Stop();
}

bool Agent::IsStarted() {
  return impl->IsStarted();
}

bool Agent::IsConnected() {
  return impl->IsConnected();
}

void Agent::WaitForDisconnect() {
  impl->WaitForDisconnect();
}

}  // namespace inspector
}  // namespace node

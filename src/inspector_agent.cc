#include "inspector_agent.h"

#include "inspector_socket.h"
#include "env-inl.h"
#include "node.h"
#include "node_crypto.h"
#include "node_mutex.h"
#include "node_version.h"
#include "v8-platform.h"
#include "util.h"
#include "zlib.h"

#include "platform/v8_inspector/public/InspectorVersion.h"
#include "platform/v8_inspector/public/V8Inspector.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/v8_inspector/public/V8StackTrace.h"
#include "platform/inspector_protocol/InspectorProtocol.h"

#include "libplatform/libplatform.h"

#include <map>
#include <sstream>
#include <string.h>
#include <utility>
#include <vector>


namespace node {
namespace inspector {
namespace {

const char TAG_CONNECT[] = "#connect";
const char TAG_DISCONNECT[] = "#disconnect";

static const uint8_t PROTOCOL_JSON[] = {
#include "v8_inspector_protocol_json.h"  // NOLINT(build/include_order)
};

std::string GetWsUrl(const sockaddr_in& address, const std::string& id) {
  char buf[1024];
  char name[64];

  if (uv_ip4_name(&address, name, sizeof(name))) *name = '\0';
  const int port = ntohs(address.sin_port);
  snprintf(buf, sizeof(buf), "%s:%d/%s", name, port, id.c_str());

  return buf;
}

void PrintDebuggerReadyMessage(const sockaddr_in& address,
                               const std::string& id) {
  const std::string ws_url = GetWsUrl(address, id);
  const size_t slash_pos = ws_url.find('/');
  CHECK_NE(slash_pos, std::string::npos);

  fprintf(stderr, "Debugger listening on %.*s.\n"
    "Warning: This is an experimental feature and could change at any time.\n"
    "To start debugging, open the following URL in Chrome:\n"
    "    chrome-devtools://devtools/remote/serve_file/"
    "@" V8_INSPECTOR_REVISION "/inspector.html?"
    "experiments=true&v8only=true&ws=%s\n",
    static_cast<int>(slash_pos), ws_url.c_str(), ws_url.c_str());
  fflush(stderr);
}

std::string MapToString(const std::map<std::string, std::string> object) {
  std::ostringstream json;
  json << "[ {\n";
  bool first = true;
  for (const auto& name_value : object) {
    if (!first)
      json << ",\n";
    json << "  \"" << name_value.first << "\": \"";
    json << name_value.second << "\"";
    first = false;
  }
  json << "\n} ]\n\n";
  return json.str();
}

void Escape(std::string* string) {
  for (char& c : *string) {
    c = (c == '\"' || c == '\\') ? '_' : c;
  }
}

void DisposeInspector(InspectorSocket* socket, int status) {
  delete socket;
}

void DisconnectAndDisposeIO(InspectorSocket* socket) {
  if (socket) {
    inspector_close(socket, DisposeInspector);
  }
}

void OnBufferAlloc(uv_handle_t* handle, size_t len, uv_buf_t* buf) {
  buf->base = new char[len];
  buf->len = len;
}

void SendHttpResponse(InspectorSocket* socket, const char* response,
                      size_t size) {
  const char HEADERS[] = "HTTP/1.0 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n";
  char header[sizeof(HEADERS) + 20];
  int header_len = snprintf(header, sizeof(header), HEADERS, size);
  inspector_write(socket, header, header_len);
  inspector_write(socket, response, size);
}

void SendHttpResponse(InspectorSocket* socket, const std::string& response) {
  SendHttpResponse(socket, response.data(), response.size());
}

void SendVersionResponse(InspectorSocket* socket) {
  static const char response[] =
      "{\n"
      "  \"Browser\": \"node.js/" NODE_VERSION "\",\n"
      "  \"Protocol-Version\": \"1.1\"\n"
      "}\n";
  SendHttpResponse(socket, response, sizeof(response) - 1);
}

std::string GetProcessTitle() {
  // uv_get_process_title will trim the title if it is too long.
  char title[2048];
  int err = uv_get_process_title(title, sizeof(title));
  if (err == 0) {
    return title;
  } else {
    return "Node.js";
  }
}

void SendProtocolJson(InspectorSocket* socket) {
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  CHECK_EQ(Z_OK, inflateInit(&strm));
  static const size_t kDecompressedSize =
      PROTOCOL_JSON[0] * 0x10000u +
      PROTOCOL_JSON[1] * 0x100u +
      PROTOCOL_JSON[2];
  strm.next_in = const_cast<uint8_t*>(PROTOCOL_JSON + 3);
  strm.avail_in = sizeof(PROTOCOL_JSON) - 3;
  std::string data(kDecompressedSize, '\0');
  strm.next_out = reinterpret_cast<Byte*>(&data[0]);
  strm.avail_out = data.size();
  CHECK_EQ(Z_STREAM_END, inflate(&strm, Z_FINISH));
  CHECK_EQ(0, strm.avail_out);
  CHECK_EQ(Z_OK, inflateEnd(&strm));
  SendHttpResponse(socket, data);
}

const char* match_path_segment(const char* path, const char* expected) {
  size_t len = strlen(expected);
  if (StringEqualNoCaseN(path, expected, len)) {
    if (path[len] == '/') return path + len + 1;
    if (path[len] == '\0') return path + len;
  }
  return nullptr;
}

// UUID RFC: https://www.ietf.org/rfc/rfc4122.txt
// Used ver 4 - with numbers
std::string GenerateID() {
  uint16_t buffer[8];
  CHECK(crypto::EntropySource(reinterpret_cast<unsigned char*>(buffer),
                              sizeof(buffer)));

  char uuid[256];
  snprintf(uuid, sizeof(uuid), "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
           buffer[0],  // time_low
           buffer[1],  // time_mid
           buffer[2],  // time_low
           (buffer[3] & 0x0fff) | 0x4000,  // time_hi_and_version
           (buffer[4] & 0x3fff) | 0x8000,  // clk_seq_hi clk_seq_low
           buffer[5],  // node
           buffer[6],
           buffer[7]);
  return uuid;
}
}  // namespace


class V8NodeInspector;

class AgentImpl {
 public:
  explicit AgentImpl(node::Environment* env);
  ~AgentImpl();

  // Start the inspector agent thread
  bool Start(v8::Platform* platform, const char* path,
             const char* host, int port, bool wait);
  // Stop the inspector agent
  void Stop();

  bool IsStarted();
  bool IsConnected() {  return state_ == State::kConnected; }
  void WaitForDisconnect();

  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);

 private:
  using MessageQueue = std::vector<std::pair<int, String16>>;
  enum class State { kNew, kAccepting, kConnected, kDone, kError };

  static void ThreadCbIO(void* agent);
  static void OnSocketConnectionIO(uv_stream_t* server, int status);
  static bool OnInspectorHandshakeIO(InspectorSocket* socket,
                                     enum inspector_handshake_event state,
                                     const std::string& path);
  static void WriteCbIO(uv_async_t* async);

  void InstallInspectorOnProcess();

  void WorkerRunIO();
  void OnInspectorConnectionIO(InspectorSocket* socket);
  void OnRemoteDataIO(InspectorSocket* stream, ssize_t read,
                      const uv_buf_t* b);
  void SetConnected(bool connected);
  void DispatchMessages();
  void Write(int session_id, const String16& message);
  bool AppendMessage(MessageQueue* vector, int session_id,
                     const String16& message);
  void SwapBehindLock(MessageQueue* vector1, MessageQueue* vector2);
  void PostIncomingMessage(const String16& message);
  void WaitForFrontendMessage();
  void NotifyMessageReceived();
  State ToState(State state);
  void SendListResponse(InspectorSocket* socket);
  bool RespondToGet(InspectorSocket* socket, const std::string& path);

  uv_sem_t start_sem_;
  ConditionVariable incoming_message_cond_;
  Mutex state_lock_;
  uv_thread_t thread_;
  uv_loop_t child_loop_;

  struct sockaddr_in saddr_;
  bool wait_;
  bool shutting_down_;
  State state_;
  node::Environment* parent_env_;

  uv_async_t* data_written_;
  uv_async_t io_thread_req_;
  InspectorSocket* client_socket_;
  V8NodeInspector* inspector_;
  v8::Platform* platform_;
  MessageQueue incoming_message_queue_;
  MessageQueue outgoing_message_queue_;
  bool dispatching_messages_;
  int frontend_session_id_;
  int backend_session_id_;

  std::string script_name_;
  std::string script_path_;
  const std::string id_;

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
  InspectorSocket* socket = inspector_from_stream(stream);
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

using V8Inspector = v8_inspector::V8Inspector;

class V8NodeInspector : public v8_inspector::V8InspectorClient {
 public:
  V8NodeInspector(AgentImpl* agent, node::Environment* env,
                  v8::Platform* platform)
                  : agent_(agent),
                    env_(env),
                    platform_(platform),
                    terminated_(false),
                    running_nested_loop_(false),
                    inspector_(V8Inspector::create(env->isolate(), this)) {
    inspector_->contextCreated(
        v8_inspector::V8ContextInfo(env->context(), 1, "NodeJS Main Context"));
  }

  void runMessageLoopOnPause(int context_group_id) override {
    if (running_nested_loop_)
      return;
    terminated_ = false;
    running_nested_loop_ = true;
    while (!terminated_) {
      agent_->WaitForFrontendMessage();
      while (v8::platform::PumpMessageLoop(platform_, env_->isolate()))
        {}
    }
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

  v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId)
      override {
    return env_->context();
  }

  V8Inspector* inspector() {
    return inspector_.get();
  }

 private:
  AgentImpl* agent_;
  node::Environment* env_;
  v8::Platform* platform_;
  bool terminated_;
  bool running_nested_loop_;
  std::unique_ptr<V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
};

AgentImpl::AgentImpl(Environment* env) : saddr_(),
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
                                         backend_session_id_(0),
                                         id_(GenerateID()) {
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
  static_cast<void>(node_method.As<v8::Function>()->Call(context,
                                                         info.Holder(),
                                                         call_args.size(),
                                                         call_args.data()));
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

bool AgentImpl::Start(v8::Platform* platform, const char* path,
                      const char* address, int port, bool wait) {
  if (*address == '\0') address = "127.0.0.1";
  if (uv_ip4_addr(address, port, &saddr_)) return false;

  auto env = parent_env_;
  inspector_ = new V8NodeInspector(this, env, platform);
  platform_ = platform;
  if (path != nullptr)
    script_name_ = path;

  InstallInspectorOnProcess();

  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);

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
  if (state_ == State::kConnected) {
    shutting_down_ = true;
    fprintf(stderr, "Waiting for the debugger to disconnect...\n");
    fflush(stderr);
    inspector_->runMessageLoopOnPause(0);
  }
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

String16 ToProtocolString(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined() ||
      !value->IsString()) {
    return String16();
  }
  v8::Local<v8::String> string_value = v8::Local<v8::String>::Cast(value);
  std::basic_string<uint16_t> buffer(string_value->Length(), '\0');
  string_value->Write(&buffer[0], 0, string_value->Length());
  return String16(buffer);
}

void AgentImpl::FatalException(v8::Local<v8::Value> error,
                               v8::Local<v8::Message> message) {
  if (!IsStarted())
    return;
  auto env = parent_env_;
  v8::Local<v8::Context> context = env->context();

  int script_id = message->GetScriptOrigin().ScriptID()->Value();
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      inspector_->inspector()->createStackTrace(message->GetStackTrace());

  if (stack_trace && !stack_trace->isEmpty() &&
      String16::fromInteger(script_id) == stack_trace->topScriptId()) {
    script_id = 0;
  }

  inspector_->inspector()->exceptionThrown(
      context,
      "Uncaught",
      error,
      ToProtocolString(message->Get()),
      ToProtocolString(message->GetScriptResourceName()),
      message->GetLineNumber(context).FromMaybe(0),
      message->GetStartColumn(context).FromMaybe(0),
      std::move(stack_trace),
      script_id);
  WaitForDisconnect();
}

// static
void AgentImpl::ThreadCbIO(void* agent) {
  static_cast<AgentImpl*>(agent)->WorkerRunIO();
}

// static
void AgentImpl::OnSocketConnectionIO(uv_stream_t* server, int status) {
  if (status == 0) {
    InspectorSocket* socket = new InspectorSocket();
    socket->data = server->data;
    if (inspector_accept(server, socket,
                         AgentImpl::OnInspectorHandshakeIO) != 0) {
      delete socket;
    }
  }
}

// static
bool AgentImpl::OnInspectorHandshakeIO(InspectorSocket* socket,
                                       enum inspector_handshake_event state,
                                       const std::string& path) {
  AgentImpl* agent = static_cast<AgentImpl*>(socket->data);
  switch (state) {
  case kInspectorHandshakeHttpGet:
    return agent->RespondToGet(socket, path);
  case kInspectorHandshakeUpgrading:
    return path.length() == agent->id_.length() + 1 &&
           path.find(agent->id_) == 1;
  case kInspectorHandshakeUpgraded:
    agent->OnInspectorConnectionIO(socket);
    return true;
  case kInspectorHandshakeFailed:
    delete socket;
    return false;
  default:
    UNREACHABLE();
    return false;
  }
}

void AgentImpl::OnRemoteDataIO(InspectorSocket* socket,
                               ssize_t read,
                               const uv_buf_t* buf) {
  if (read > 0) {
    String16 str = String16::fromUTF8(buf->base, read);
    // TODO(pfeldman): Instead of blocking execution while debugger
    // engages, node should wait for the run callback from the remote client
    // and initiate its startup. This is a change to node.cc that should be
    // upstreamed separately.
    if (wait_&& str.find("\"Runtime.runIfWaitingForDebugger\"")
        != std::string::npos) {
      wait_ = false;
      uv_sem_post(&start_sem_);
    }
    PostIncomingMessage(str);
  } else {
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
}

void AgentImpl::SendListResponse(InspectorSocket* socket) {
  std::map<std::string, std::string> response;
  response["description"] = "node.js instance";
  response["faviconUrl"] = "https://nodejs.org/static/favicon.ico";
  response["id"] = id_;
  response["title"] = script_name_.empty() ? GetProcessTitle() : script_name_;
  Escape(&response["title"]);
  response["type"] = "node";
  // This attribute value is a "best effort" URL that is passed as a JSON
  // string. It is not guaranteed to resolve to a valid resource.
  response["url"] = "file://" + script_path_;
  Escape(&response["url"]);

  if (!client_socket_) {
    std::string address = GetWsUrl(saddr_, id_);

    std::ostringstream frontend_url;
    frontend_url << "https://chrome-devtools-frontend.appspot.com/serve_file/@";
    frontend_url << V8_INSPECTOR_REVISION;
    frontend_url << "/inspector.html?experiments=true&v8only=true&ws=";
    frontend_url << address;

    response["devtoolsFrontendUrl"] += frontend_url.str();
    response["webSocketDebuggerUrl"] = "ws://" + address;
  }
  SendHttpResponse(socket, MapToString(response));
}

bool AgentImpl::RespondToGet(InspectorSocket* socket, const std::string& path) {
  const char* command = match_path_segment(path.c_str(), "/json");
  if (command == nullptr)
    return false;

  if (match_path_segment(command, "list") || command[0] == '\0') {
    SendListResponse(socket);
    return true;
  } else if (match_path_segment(command, "protocol")) {
    SendProtocolJson(socket);
    return true;
  } else if (match_path_segment(command, "version")) {
    SendVersionResponse(socket);
    return true;
  } else if (const char* pid = match_path_segment(command, "activate")) {
    if (pid != id_)
      return false;
    SendHttpResponse(socket, "Target activated");
    return true;
  }
  return false;
}

// static
void AgentImpl::WriteCbIO(uv_async_t* async) {
  AgentImpl* agent = static_cast<AgentImpl*>(async->data);
  InspectorSocket* socket = agent->client_socket_;
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
  uv_tcp_t server;
  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);
  err = uv_async_init(&child_loop_, &io_thread_req_, AgentImpl::WriteCbIO);
  CHECK_EQ(err, 0);
  io_thread_req_.data = this;
  if (!script_name_.empty()) {
    uv_fs_t req;
    if (0 == uv_fs_realpath(&child_loop_, &req, script_name_.c_str(), nullptr))
      script_path_ = std::string(reinterpret_cast<char*>(req.ptr));
    uv_fs_req_cleanup(&req);
  }
  uv_tcp_init(&child_loop_, &server);
  server.data = this;
  err = uv_tcp_bind(&server,
                    reinterpret_cast<const struct sockaddr*>(&saddr_), 0);
  if (err == 0) {
    err = uv_listen(reinterpret_cast<uv_stream_t*>(&server), 1,
                    OnSocketConnectionIO);
  }
  if (err == 0) {
    int namelen = sizeof(saddr_);
    err = uv_tcp_getsockname(&server, reinterpret_cast<sockaddr*>(&saddr_),
                             &namelen);
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
  PrintDebuggerReadyMessage(saddr_, id_);
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

bool AgentImpl::AppendMessage(MessageQueue* queue, int session_id,
                              const String16& message) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  bool trigger_pumping = queue->empty();
  queue->push_back(std::make_pair(session_id, message));
  return trigger_pumping;
}

void AgentImpl::SwapBehindLock(MessageQueue* vector1, MessageQueue* vector2) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  vector1->swap(*vector2);
}

void AgentImpl::PostIncomingMessage(const String16& message) {
  if (AppendMessage(&incoming_message_queue_, frontend_session_id_, message)) {
    v8::Isolate* isolate = parent_env_->isolate();
    platform_->CallOnForegroundThread(isolate,
                                      new DispatchOnInspectorBackendTask(this));
    isolate->RequestInterrupt(InterruptCallback, this);
    uv_async_send(data_written_);
  }
  NotifyMessageReceived();
}

void AgentImpl::WaitForFrontendMessage() {
  Mutex::ScopedLock scoped_lock(state_lock_);
  if (incoming_message_queue_.empty())
    incoming_message_cond_.Wait(scoped_lock);
}

void AgentImpl::NotifyMessageReceived() {
  Mutex::ScopedLock scoped_lock(state_lock_);
  incoming_message_cond_.Broadcast(scoped_lock);
}

void AgentImpl::OnInspectorConnectionIO(InspectorSocket* socket) {
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
  // This function can be reentered if there was an incoming message while
  // V8 was processing another inspector request (e.g. if the user is
  // evaluating a long-running JS code snippet). This can happen only at
  // specific points (e.g. the lines that call inspector_ methods)
  if (dispatching_messages_)
    return;
  dispatching_messages_ = true;
  MessageQueue tasks;
  do {
    tasks.clear();
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
          PrintDebuggerReadyMessage(saddr_, id_);
          state_ = State::kAccepting;
        }
        inspector_->quitMessageLoopOnPause();
        inspector_->disconnectFrontend();
      } else {
        inspector_->dispatchMessageFromFrontend(message);
      }
    }
  } while (!tasks.empty());
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

bool Agent::Start(v8::Platform* platform, const char* path,
                  const char* host, int port, bool wait) {
  return impl->Start(platform, path, host, port, wait);
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

void Agent::FatalException(v8::Local<v8::Value> error,
                           v8::Local<v8::Message> message) {
  impl->FatalException(error, message);
}


}  // namespace inspector
}  // namespace node

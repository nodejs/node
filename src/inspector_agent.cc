#include "inspector_agent.h"

#include "inspector_socket_server.h"
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "node_crypto.h"
#include "node_mutex.h"
#include "node_version.h"
#include "v8-inspector.h"
#include "v8-platform.h"
#include "util.h"
#include "zlib.h"

#include "libplatform/libplatform.h"

#include <map>
#include <sstream>
#include <tuple>
#include <unicode/unistr.h>

#include <string.h>
#include <utility>
#include <vector>


namespace node {
namespace inspector {
namespace {

using v8_inspector::StringBuffer;
using v8_inspector::StringView;

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

std::string StringViewToUtf8(const StringView& view) {
  if (view.is8Bit()) {
    return std::string(reinterpret_cast<const char*>(view.characters8()),
                       view.length());
  }
  const uint16_t* source = view.characters16();
  const UChar* unicodeSource = reinterpret_cast<const UChar*>(source);
  static_assert(sizeof(*source) == sizeof(*unicodeSource),
                "sizeof(*source) == sizeof(*unicodeSource)");

  size_t result_length = view.length() * sizeof(*source);
  std::string result(result_length, '\0');
  UnicodeString utf16(unicodeSource, view.length());
  // ICU components for std::string compatibility are not enabled in build...
  bool done = false;
  while (!done) {
    CheckedArrayByteSink sink(&result[0], result_length);
    utf16.toUTF8(sink);
    result_length = sink.NumberOfBytesAppended();
    result.resize(result_length);
    done = !sink.Overflowed();
  }
  return result;
}

std::unique_ptr<StringBuffer> Utf8ToStringView(const std::string& message) {
  UnicodeString utf16 =
      UnicodeString::fromUTF8(StringPiece(message.data(), message.length()));
  StringView view(reinterpret_cast<const uint16_t*>(utf16.getBuffer()),
                  utf16.length());
  return StringBuffer::create(view);
}

}  // namespace

class V8NodeInspector;

enum class InspectorAction {
  kStartSession, kEndSession, kSendMessage
};

enum class TransportAction {
  kSendMessage, kStop
};

class InspectorAgentDelegate: public node::inspector::SocketServerDelegate {
 public:
  InspectorAgentDelegate(AgentImpl* agent, const std::string& script_path,
                         const std::string& script_name, bool wait);
  bool StartSession(int session_id, const std::string& target_id) override;
  void MessageReceived(int session_id, const std::string& message) override;
  void EndSession(int session_id) override;
  std::vector<std::string> GetTargetIds() override;
  std::string GetTargetTitle(const std::string& id) override;
  std::string GetTargetUrl(const std::string& id) override;
  bool IsConnected() { return connected_; }
 private:
  AgentImpl* agent_;
  bool connected_;
  int session_id_;
  const std::string script_name_;
  const std::string script_path_;
  const std::string target_id_;
  bool waiting_;
};

class AgentImpl {
 public:
  explicit AgentImpl(node::Environment* env);

  // Start the inspector agent thread
  bool Start(v8::Platform* platform, const char* path,
             const DebugOptions& options);
  // Stop the inspector agent
  void Stop();

  bool IsStarted();
  bool IsConnected();
  void WaitForDisconnect();

  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);

  void PostIncomingMessage(InspectorAction action, int session_id,
                           const std::string& message);
  void ResumeStartup() {
    uv_sem_post(&start_sem_);
  }

 private:
  template <typename Action>
  using MessageQueue =
      std::vector<std::tuple<Action, int, std::unique_ptr<StringBuffer>>>;
  enum class State { kNew, kAccepting, kConnected, kDone, kError };

  static void ThreadCbIO(void* agent);
  static void WriteCbIO(uv_async_t* async);
  static void MainThreadAsyncCb(uv_async_t* req);

  void InstallInspectorOnProcess();

  void WorkerRunIO();
  void SetConnected(bool connected);
  void DispatchMessages();
  void Write(TransportAction action, int session_id, const StringView& message);
  template <typename ActionType>
  bool AppendMessage(MessageQueue<ActionType>* vector, ActionType action,
                     int session_id, std::unique_ptr<StringBuffer> buffer);
  template <typename ActionType>
  void SwapBehindLock(MessageQueue<ActionType>* vector1,
                      MessageQueue<ActionType>* vector2);
  void WaitForFrontendMessage();
  void NotifyMessageReceived();
  State ToState(State state);

  DebugOptions options_;
  uv_sem_t start_sem_;
  ConditionVariable incoming_message_cond_;
  Mutex state_lock_;
  uv_thread_t thread_;
  uv_loop_t child_loop_;

  InspectorAgentDelegate* delegate_;
  bool wait_;
  bool shutting_down_;
  State state_;
  node::Environment* parent_env_;

  uv_async_t io_thread_req_;
  uv_async_t main_thread_req_;
  V8NodeInspector* inspector_;
  v8::Platform* platform_;
  MessageQueue<InspectorAction> incoming_message_queue_;
  MessageQueue<TransportAction> outgoing_message_queue_;
  bool dispatching_messages_;
  int session_id_;
  InspectorSocketServer* server_;

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

class DispatchOnInspectorBackendTask : public v8::Task {
 public:
  explicit DispatchOnInspectorBackendTask(AgentImpl* agent) : agent_(agent) {}

  void Run() override {
    agent_->DispatchMessages();
  }

 private:
  AgentImpl* agent_;
};

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit ChannelImpl(AgentImpl* agent): agent_(agent) {}
  virtual ~ChannelImpl() {}
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
    agent_->Write(TransportAction::kSendMessage, agent_->session_id_, message);
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
    const uint8_t CONTEXT_NAME[] = "Node.js Main Context";
    StringView context_name(CONTEXT_NAME, sizeof(CONTEXT_NAME) - 1);
    v8_inspector::V8ContextInfo info(env->context(), 1, context_name);
    inspector_->contextCreated(info);
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
    session_ = inspector_->connect(1, new ChannelImpl(agent_), StringView());
  }

  void disconnectFrontend() {
    session_.reset();
  }

  void dispatchMessageFromFrontend(const StringView& message) {
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

AgentImpl::AgentImpl(Environment* env) : delegate_(nullptr),
                                         wait_(false),
                                         shutting_down_(false),
                                         state_(State::kNew),
                                         parent_env_(env),
                                         inspector_(nullptr),
                                         platform_(nullptr),
                                         dispatching_messages_(false),
                                         session_id_(0),
                                         server_(nullptr) {
  CHECK_EQ(0, uv_async_init(env->event_loop(), &main_thread_req_,
                            AgentImpl::MainThreadAsyncCb));
  uv_unref(reinterpret_cast<uv_handle_t*>(&main_thread_req_));
  CHECK_EQ(0, uv_sem_init(&start_sem_, 0));
  memset(&io_thread_req_, 0, sizeof(io_thread_req_));
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
                      const DebugOptions& options) {
  options_ = options;
  wait_ = options.wait_for_connect();

  auto env = parent_env_;
  inspector_ = new V8NodeInspector(this, env, platform);
  platform_ = platform;
  if (path != nullptr)
    script_name_ = path;

  InstallInspectorOnProcess();

  int err = uv_thread_create(&thread_, AgentImpl::ThreadCbIO, this);
  CHECK_EQ(err, 0);
  uv_sem_wait(&start_sem_);

  if (state_ == State::kError) {
    Stop();
    return false;
  }
  state_ = State::kAccepting;
  if (options_.wait_for_connect()) {
    DispatchMessages();
  }
  return true;
}

void AgentImpl::Stop() {
  int err = uv_thread_join(&thread_);
  CHECK_EQ(err, 0);
  delete inspector_;
}

bool AgentImpl::IsConnected() {
  return delegate_ != nullptr && delegate_->IsConnected();
}

bool AgentImpl::IsStarted() {
  return !!platform_;
}

void AgentImpl::WaitForDisconnect() {
  if (state_ == State::kConnected) {
    shutting_down_ = true;
    Write(TransportAction::kStop, 0, StringView());
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

std::unique_ptr<StringBuffer> ToProtocolString(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined() ||
      !value->IsString()) {
    return StringBuffer::create(StringView());
  }
  v8::Local<v8::String> string_value = v8::Local<v8::String>::Cast(value);
  size_t len = string_value->Length();
  std::basic_string<uint16_t> buffer(len, '\0');
  string_value->Write(&buffer[0], 0, len);
  return StringBuffer::create(StringView(buffer.data(), len));
}

void AgentImpl::FatalException(v8::Local<v8::Value> error,
                               v8::Local<v8::Message> message) {
  if (!IsStarted())
    return;
  auto env = parent_env_;
  v8::Local<v8::Context> context = env->context();

  int script_id = message->GetScriptOrigin().ScriptID()->Value();

  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();

  if (!stack_trace.IsEmpty() &&
      stack_trace->GetFrameCount() > 0 &&
      script_id == stack_trace->GetFrame(0)->GetScriptId()) {
    script_id = 0;
  }

  const uint8_t DETAILS[] = "Uncaught";

  inspector_->inspector()->exceptionThrown(
      context,
      StringView(DETAILS, sizeof(DETAILS) - 1),
      error,
      ToProtocolString(message->Get())->string(),
      ToProtocolString(message->GetScriptResourceName())->string(),
      message->GetLineNumber(context).FromMaybe(0),
      message->GetStartColumn(context).FromMaybe(0),
      inspector_->inspector()->createStackTrace(stack_trace),
      script_id);
  WaitForDisconnect();
}

// static
void AgentImpl::ThreadCbIO(void* agent) {
  static_cast<AgentImpl*>(agent)->WorkerRunIO();
}

// static
void AgentImpl::WriteCbIO(uv_async_t* async) {
  AgentImpl* agent = static_cast<AgentImpl*>(async->data);
  MessageQueue<TransportAction> outgoing_messages;
  agent->SwapBehindLock(&agent->outgoing_message_queue_, &outgoing_messages);
  for (const auto& outgoing : outgoing_messages) {
    switch (std::get<0>(outgoing)) {
    case TransportAction::kStop:
      agent->server_->Stop(nullptr);
      break;
    case TransportAction::kSendMessage:
      std::string message = StringViewToUtf8(std::get<2>(outgoing)->string());
      agent->server_->Send(std::get<1>(outgoing), message);
      break;
    }
  }
}

void AgentImpl::WorkerRunIO() {
  int err = uv_loop_init(&child_loop_);
  CHECK_EQ(err, 0);
  err = uv_async_init(&child_loop_, &io_thread_req_, AgentImpl::WriteCbIO);
  CHECK_EQ(err, 0);
  io_thread_req_.data = this;
  std::string script_path;
  if (!script_name_.empty()) {
    uv_fs_t req;
    if (0 == uv_fs_realpath(&child_loop_, &req, script_name_.c_str(), nullptr))
      script_path = std::string(reinterpret_cast<char*>(req.ptr));
    uv_fs_req_cleanup(&req);
  }
  InspectorAgentDelegate delegate(this, script_path, script_name_, wait_);
  delegate_ = &delegate;
  InspectorSocketServer server(&delegate,
                               options_.host_name(),
                               options_.port());
  if (!server.Start(&child_loop_)) {
    state_ = State::kError;  // Safe, main thread is waiting on semaphore
    uv_close(reinterpret_cast<uv_handle_t*>(&io_thread_req_), nullptr);
    uv_loop_close(&child_loop_);
    uv_sem_post(&start_sem_);
    return;
  }
  server_ = &server;
  if (!wait_) {
    uv_sem_post(&start_sem_);
  }
  uv_run(&child_loop_, UV_RUN_DEFAULT);
  uv_close(reinterpret_cast<uv_handle_t*>(&io_thread_req_), nullptr);
  server.Stop(nullptr);
  server.TerminateConnections(nullptr);
  uv_run(&child_loop_, UV_RUN_NOWAIT);
  err = uv_loop_close(&child_loop_);
  CHECK_EQ(err, 0);
  delegate_ = nullptr;
  server_ = nullptr;
}

template <typename ActionType>
bool AgentImpl::AppendMessage(MessageQueue<ActionType>* queue,
                              ActionType action, int session_id,
                              std::unique_ptr<StringBuffer> buffer) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  bool trigger_pumping = queue->empty();
  queue->push_back(std::make_tuple(action, session_id, std::move(buffer)));
  return trigger_pumping;
}

template <typename ActionType>
void AgentImpl::SwapBehindLock(MessageQueue<ActionType>* vector1,
                               MessageQueue<ActionType>* vector2) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  vector1->swap(*vector2);
}

void AgentImpl::PostIncomingMessage(InspectorAction action, int session_id,
                                    const std::string& message) {
  if (AppendMessage(&incoming_message_queue_, action, session_id,
                    Utf8ToStringView(message))) {
    v8::Isolate* isolate = parent_env_->isolate();
    platform_->CallOnForegroundThread(isolate,
                                      new DispatchOnInspectorBackendTask(this));
    isolate->RequestInterrupt(InterruptCallback, this);
    CHECK_EQ(0, uv_async_send(&main_thread_req_));
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

void AgentImpl::DispatchMessages() {
  // This function can be reentered if there was an incoming message while
  // V8 was processing another inspector request (e.g. if the user is
  // evaluating a long-running JS code snippet). This can happen only at
  // specific points (e.g. the lines that call inspector_ methods)
  if (dispatching_messages_)
    return;
  dispatching_messages_ = true;
  MessageQueue<InspectorAction> tasks;
  do {
    tasks.clear();
    SwapBehindLock(&incoming_message_queue_, &tasks);
    for (const auto& task : tasks) {
      StringView message = std::get<2>(task)->string();
      switch (std::get<0>(task)) {
      case InspectorAction::kStartSession:
        CHECK_EQ(State::kAccepting, state_);
        session_id_ = std::get<1>(task);
        state_ = State::kConnected;
        fprintf(stderr, "Debugger attached.\n");
        inspector_->connectFrontend();
        break;
      case InspectorAction::kEndSession:
        CHECK_EQ(State::kConnected, state_);
        if (shutting_down_) {
          state_ = State::kDone;
        } else {
          state_ = State::kAccepting;
        }
        inspector_->quitMessageLoopOnPause();
        inspector_->disconnectFrontend();
        break;
      case InspectorAction::kSendMessage:
        inspector_->dispatchMessageFromFrontend(message);
        break;
      }
    }
  } while (!tasks.empty());
  dispatching_messages_ = false;
}

// static
void AgentImpl::MainThreadAsyncCb(uv_async_t* req) {
  AgentImpl* agent = node::ContainerOf(&AgentImpl::main_thread_req_, req);
  agent->DispatchMessages();
}

void AgentImpl::Write(TransportAction action, int session_id,
                      const StringView& inspector_message) {
  AppendMessage(&outgoing_message_queue_, action, session_id,
                StringBuffer::create(inspector_message));
  int err = uv_async_send(&io_thread_req_);
  CHECK_EQ(0, err);
}

// Exported class Agent
Agent::Agent(node::Environment* env) : impl(new AgentImpl(env)) {}

Agent::~Agent() {
  delete impl;
}

bool Agent::Start(v8::Platform* platform, const char* path,
                  const DebugOptions& options) {
  return impl->Start(platform, path, options);
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

InspectorAgentDelegate::InspectorAgentDelegate(AgentImpl* agent,
                                               const std::string& script_path,
                                               const std::string& script_name,
                                               bool wait)
                                               : agent_(agent),
                                                 connected_(false),
                                                 session_id_(0),
                                                 script_name_(script_name),
                                                 script_path_(script_path),
                                                 target_id_(GenerateID()),
                                                 waiting_(wait) { }


bool InspectorAgentDelegate::StartSession(int session_id,
                                          const std::string& target_id) {
  if (connected_)
    return false;
  connected_ = true;
  session_id_++;
  agent_->PostIncomingMessage(InspectorAction::kStartSession, session_id, "");
  return true;
}

void InspectorAgentDelegate::MessageReceived(int session_id,
                                             const std::string& message) {
  // TODO(pfeldman): Instead of blocking execution while debugger
  // engages, node should wait for the run callback from the remote client
  // and initiate its startup. This is a change to node.cc that should be
  // upstreamed separately.
  if (waiting_) {
    if (message.find("\"Runtime.runIfWaitingForDebugger\"") !=
        std::string::npos) {
      waiting_ = false;
      agent_->ResumeStartup();
    }
  }
  agent_->PostIncomingMessage(InspectorAction::kSendMessage, session_id,
                              message);
}

void InspectorAgentDelegate::EndSession(int session_id) {
  connected_ = false;
  agent_->PostIncomingMessage(InspectorAction::kEndSession, session_id, "");
}

std::vector<std::string> InspectorAgentDelegate::GetTargetIds() {
  return { target_id_ };
}

std::string InspectorAgentDelegate::GetTargetTitle(const std::string& id) {
  return script_name_.empty() ? GetProcessTitle() : script_name_;
}

std::string InspectorAgentDelegate::GetTargetUrl(const std::string& id) {
  return "file://" + script_path_;
}

}  // namespace inspector
}  // namespace node

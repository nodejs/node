#include "inspector_io.h"

#include "inspector_socket_server.h"
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "node_crypto.h"
#include "node_mutex.h"
#include "v8-inspector.h"
#include "util.h"
#include "zlib.h"

#include <sstream>
#include <unicode/unistr.h>

#include <string.h>
#include <vector>


namespace node {
namespace inspector {
namespace {
using AsyncAndAgent = std::pair<uv_async_t, Agent*>;
using v8_inspector::StringBuffer;
using v8_inspector::StringView;

template<typename Transport>
using TransportAndIo = std::pair<Transport*, InspectorIo*>;

std::string GetProcessTitle() {
  char title[2048];
  int err = uv_get_process_title(title, sizeof(title));
  if (err == 0) {
    return title;
  } else {
    // Title is too long, or could not be retrieved.
    return "Node.js";
  }
}

std::string ScriptPath(uv_loop_t* loop, const std::string& script_name) {
  std::string script_path;

  if (!script_name.empty()) {
    uv_fs_t req;
    req.ptr = nullptr;
    if (0 == uv_fs_realpath(loop, &req, script_name.c_str(), nullptr)) {
      CHECK_NE(req.ptr, nullptr);
      script_path = std::string(static_cast<char*>(req.ptr));
    }
    uv_fs_req_cleanup(&req);
  }

  return script_path;
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

void HandleSyncCloseCb(uv_handle_t* handle) {
  *static_cast<bool*>(handle->data) = true;
}

int CloseAsyncAndLoop(uv_async_t* async) {
  bool is_closed = false;
  async->data = &is_closed;
  uv_close(reinterpret_cast<uv_handle_t*>(async), HandleSyncCloseCb);
  while (!is_closed)
    uv_run(async->loop, UV_RUN_ONCE);
  async->data = nullptr;
  return uv_loop_close(async->loop);
}

// Delete main_thread_req_ on async handle close
void ReleasePairOnAsyncClose(uv_handle_t* async) {
  AsyncAndAgent* pair = node::ContainerOf(&AsyncAndAgent::first,
                                          reinterpret_cast<uv_async_t*>(async));
  delete pair;
}

}  // namespace

std::unique_ptr<StringBuffer> Utf8ToStringView(const std::string& message) {
  UnicodeString utf16 =
      UnicodeString::fromUTF8(StringPiece(message.data(), message.length()));
  StringView view(reinterpret_cast<const uint16_t*>(utf16.getBuffer()),
                  utf16.length());
  return StringBuffer::create(view);
}


class IoSessionDelegate : public InspectorSessionDelegate {
 public:
  explicit IoSessionDelegate(InspectorIo* io) : io_(io) { }
  bool WaitForFrontendMessageWhilePaused() override;
  void SendMessageToFrontend(const v8_inspector::StringView& message) override;
 private:
  InspectorIo* io_;
};

// Passed to InspectorSocketServer to handle WS inspector protocol events,
// mostly session start, message received, and session end.
class InspectorIoDelegate: public node::inspector::SocketServerDelegate {
 public:
  InspectorIoDelegate(InspectorIo* io, const std::string& script_path,
                      const std::string& script_name, bool wait);
  // Calls PostIncomingMessage() with appropriate InspectorAction:
  //   kStartSession
  bool StartSession(int session_id, const std::string& target_id) override;
  //   kSendMessage
  void MessageReceived(int session_id, const std::string& message) override;
  //   kEndSession
  void EndSession(int session_id) override;

  std::vector<std::string> GetTargetIds() override;
  std::string GetTargetTitle(const std::string& id) override;
  std::string GetTargetUrl(const std::string& id) override;
  bool IsConnected() { return connected_; }
  void ServerDone() override {
    io_->ServerDone();
  }

 private:
  InspectorIo* io_;
  bool connected_;
  int session_id_;
  const std::string script_name_;
  const std::string script_path_;
  const std::string target_id_;
  bool waiting_;
};

void InterruptCallback(v8::Isolate*, void* agent) {
  InspectorIo* io = static_cast<Agent*>(agent)->io();
  if (io != nullptr)
    io->DispatchMessages();
}

class DispatchMessagesTask : public v8::Task {
 public:
  explicit DispatchMessagesTask(Agent* agent) : agent_(agent) {}

  void Run() override {
    InspectorIo* io = agent_->io();
    if (io != nullptr)
      io->DispatchMessages();
  }

 private:
  Agent* agent_;
};

InspectorIo::InspectorIo(Environment* env, v8::Platform* platform,
                         const std::string& path, const DebugOptions& options,
                         bool wait_for_connect)
                         : options_(options), thread_(), delegate_(nullptr),
                           state_(State::kNew), parent_env_(env),
                           thread_req_(), platform_(platform),
                           dispatching_messages_(false), session_id_(0),
                           script_name_(path),
                           wait_for_connect_(wait_for_connect), port_(-1) {
  main_thread_req_ = new AsyncAndAgent({uv_async_t(), env->inspector_agent()});
  CHECK_EQ(0, uv_async_init(env->event_loop(), &main_thread_req_->first,
                            InspectorIo::MainThreadReqAsyncCb));
  uv_unref(reinterpret_cast<uv_handle_t*>(&main_thread_req_->first));
  CHECK_EQ(0, uv_sem_init(&thread_start_sem_, 0));
}

InspectorIo::~InspectorIo() {
  uv_sem_destroy(&thread_start_sem_);
  uv_close(reinterpret_cast<uv_handle_t*>(&main_thread_req_->first),
           ReleasePairOnAsyncClose);
}

bool InspectorIo::Start() {
  CHECK_EQ(state_, State::kNew);
  CHECK_EQ(uv_thread_create(&thread_, InspectorIo::ThreadMain, this), 0);
  uv_sem_wait(&thread_start_sem_);

  if (state_ == State::kError) {
    return false;
  }
  state_ = State::kAccepting;
  if (wait_for_connect_) {
    DispatchMessages();
  }
  return true;
}

void InspectorIo::Stop() {
  CHECK(state_ == State::kAccepting || state_ == State::kConnected);
  Write(TransportAction::kKill, 0, StringView());
  int err = uv_thread_join(&thread_);
  CHECK_EQ(err, 0);
  state_ = State::kShutDown;
  DispatchMessages();
}

bool InspectorIo::IsConnected() {
  return delegate_ != nullptr && delegate_->IsConnected();
}

bool InspectorIo::IsStarted() {
  return platform_ != nullptr;
}

void InspectorIo::WaitForDisconnect() {
  if (state_ == State::kAccepting)
    state_ = State::kDone;
  if (state_ == State::kConnected) {
    state_ = State::kShutDown;
    Write(TransportAction::kStop, 0, StringView());
    fprintf(stderr, "Waiting for the debugger to disconnect...\n");
    fflush(stderr);
    parent_env_->inspector_agent()->RunMessageLoop();
  }
}

// static
void InspectorIo::ThreadMain(void* io) {
  static_cast<InspectorIo*>(io)->ThreadMain<InspectorSocketServer>();
}

// static
template <typename Transport>
void InspectorIo::IoThreadAsyncCb(uv_async_t* async) {
  TransportAndIo<Transport>* transport_and_io =
      static_cast<TransportAndIo<Transport>*>(async->data);
  if (transport_and_io == nullptr) {
    return;
  }
  Transport* transport = transport_and_io->first;
  InspectorIo* io = transport_and_io->second;
  MessageQueue<TransportAction> outgoing_message_queue;
  io->SwapBehindLock(&io->outgoing_message_queue_, &outgoing_message_queue);
  for (const auto& outgoing : outgoing_message_queue) {
    switch (std::get<0>(outgoing)) {
    case TransportAction::kKill:
      transport->TerminateConnections();
      // Fallthrough
    case TransportAction::kStop:
      transport->Stop(nullptr);
      break;
    case TransportAction::kSendMessage:
      std::string message = StringViewToUtf8(std::get<2>(outgoing)->string());
      transport->Send(std::get<1>(outgoing), message);
      break;
    }
  }
}

template<typename Transport>
void InspectorIo::ThreadMain() {
  uv_loop_t loop;
  loop.data = nullptr;
  int err = uv_loop_init(&loop);
  CHECK_EQ(err, 0);
  thread_req_.data = nullptr;
  err = uv_async_init(&loop, &thread_req_, IoThreadAsyncCb<Transport>);
  CHECK_EQ(err, 0);
  std::string script_path = ScriptPath(&loop, script_name_);
  InspectorIoDelegate delegate(this, script_path, script_name_,
                               wait_for_connect_);
  delegate_ = &delegate;
  Transport server(&delegate, &loop, options_.host_name(), options_.port());
  TransportAndIo<Transport> queue_transport(&server, this);
  thread_req_.data = &queue_transport;
  if (!server.Start()) {
    state_ = State::kError;  // Safe, main thread is waiting on semaphore
    CHECK_EQ(0, CloseAsyncAndLoop(&thread_req_));
    uv_sem_post(&thread_start_sem_);
    return;
  }
  port_ = server.Port();  // Safe, main thread is waiting on semaphore.
  if (!wait_for_connect_) {
    uv_sem_post(&thread_start_sem_);
  }
  uv_run(&loop, UV_RUN_DEFAULT);
  thread_req_.data = nullptr;
  CHECK_EQ(uv_loop_close(&loop), 0);
  delegate_ = nullptr;
}

template <typename ActionType>
bool InspectorIo::AppendMessage(MessageQueue<ActionType>* queue,
                                ActionType action, int session_id,
                                std::unique_ptr<StringBuffer> buffer) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  bool trigger_pumping = queue->empty();
  queue->push_back(std::make_tuple(action, session_id, std::move(buffer)));
  return trigger_pumping;
}

template <typename ActionType>
void InspectorIo::SwapBehindLock(MessageQueue<ActionType>* vector1,
                                 MessageQueue<ActionType>* vector2) {
  Mutex::ScopedLock scoped_lock(state_lock_);
  vector1->swap(*vector2);
}

void InspectorIo::PostIncomingMessage(InspectorAction action, int session_id,
                                      const std::string& message) {
  if (AppendMessage(&incoming_message_queue_, action, session_id,
                    Utf8ToStringView(message))) {
    Agent* agent = main_thread_req_->second;
    v8::Isolate* isolate = parent_env_->isolate();
    platform_->CallOnForegroundThread(isolate,
                                      new DispatchMessagesTask(agent));
    isolate->RequestInterrupt(InterruptCallback, agent);
    CHECK_EQ(0, uv_async_send(&main_thread_req_->first));
  }
  NotifyMessageReceived();
}

std::vector<std::string> InspectorIo::GetTargetIds() const {
  return delegate_ ? delegate_->GetTargetIds() : std::vector<std::string>();
}

void InspectorIo::WaitForFrontendMessageWhilePaused() {
  dispatching_messages_ = false;
  Mutex::ScopedLock scoped_lock(state_lock_);
  if (incoming_message_queue_.empty())
    incoming_message_cond_.Wait(scoped_lock);
}

void InspectorIo::NotifyMessageReceived() {
  Mutex::ScopedLock scoped_lock(state_lock_);
  incoming_message_cond_.Broadcast(scoped_lock);
}

void InspectorIo::DispatchMessages() {
  // This function can be reentered if there was an incoming message while
  // V8 was processing another inspector request (e.g. if the user is
  // evaluating a long-running JS code snippet). This can happen only at
  // specific points (e.g. the lines that call inspector_ methods)
  if (dispatching_messages_)
    return;
  dispatching_messages_ = true;
  bool had_messages = false;
  do {
    if (dispatching_message_queue_.empty())
      SwapBehindLock(&incoming_message_queue_, &dispatching_message_queue_);
    had_messages = !dispatching_message_queue_.empty();
    while (!dispatching_message_queue_.empty()) {
      MessageQueue<InspectorAction>::value_type task;
      std::swap(dispatching_message_queue_.front(), task);
      dispatching_message_queue_.pop_front();
      StringView message = std::get<2>(task)->string();
      switch (std::get<0>(task)) {
      case InspectorAction::kStartSession:
        CHECK_EQ(session_delegate_, nullptr);
        session_id_ = std::get<1>(task);
        state_ = State::kConnected;
        fprintf(stderr, "Debugger attached.\n");
        session_delegate_ = std::unique_ptr<InspectorSessionDelegate>(
            new IoSessionDelegate(this));
        parent_env_->inspector_agent()->Connect(session_delegate_.get());
        break;
      case InspectorAction::kEndSession:
        CHECK_NE(session_delegate_, nullptr);
        if (state_ == State::kShutDown) {
          state_ = State::kDone;
        } else {
          state_ = State::kAccepting;
        }
        parent_env_->inspector_agent()->Disconnect();
        session_delegate_.reset();
        break;
      case InspectorAction::kSendMessage:
        parent_env_->inspector_agent()->Dispatch(message);
        break;
      }
    }
  } while (had_messages);
  dispatching_messages_ = false;
}

// static
void InspectorIo::MainThreadReqAsyncCb(uv_async_t* req) {
  AsyncAndAgent* pair = node::ContainerOf(&AsyncAndAgent::first, req);
  // Note that this may be called after io was closed or even after a new
  // one was created and ran.
  InspectorIo* io = pair->second->io();
  if (io != nullptr)
    io->DispatchMessages();
}

void InspectorIo::Write(TransportAction action, int session_id,
                        const StringView& inspector_message) {
  AppendMessage(&outgoing_message_queue_, action, session_id,
                StringBuffer::create(inspector_message));
  int err = uv_async_send(&thread_req_);
  CHECK_EQ(0, err);
}

InspectorIoDelegate::InspectorIoDelegate(InspectorIo* io,
                                         const std::string& script_path,
                                         const std::string& script_name,
                                         bool wait)
                                         : io_(io),
                                           connected_(false),
                                           session_id_(0),
                                           script_name_(script_name),
                                           script_path_(script_path),
                                           target_id_(GenerateID()),
                                           waiting_(wait) { }


bool InspectorIoDelegate::StartSession(int session_id,
                                       const std::string& target_id) {
  if (connected_)
    return false;
  connected_ = true;
  session_id_++;
  io_->PostIncomingMessage(InspectorAction::kStartSession, session_id, "");
  return true;
}

void InspectorIoDelegate::MessageReceived(int session_id,
                                          const std::string& message) {
  // TODO(pfeldman): Instead of blocking execution while debugger
  // engages, node should wait for the run callback from the remote client
  // and initiate its startup. This is a change to node.cc that should be
  // upstreamed separately.
  if (waiting_) {
    if (message.find("\"Runtime.runIfWaitingForDebugger\"") !=
        std::string::npos) {
      waiting_ = false;
      io_->ResumeStartup();
    }
  }
  io_->PostIncomingMessage(InspectorAction::kSendMessage, session_id,
                           message);
}

void InspectorIoDelegate::EndSession(int session_id) {
  connected_ = false;
  io_->PostIncomingMessage(InspectorAction::kEndSession, session_id, "");
}

std::vector<std::string> InspectorIoDelegate::GetTargetIds() {
  return { target_id_ };
}

std::string InspectorIoDelegate::GetTargetTitle(const std::string& id) {
  return script_name_.empty() ? GetProcessTitle() : script_name_;
}

std::string InspectorIoDelegate::GetTargetUrl(const std::string& id) {
  return "file://" + script_path_;
}

bool IoSessionDelegate::WaitForFrontendMessageWhilePaused() {
  io_->WaitForFrontendMessageWhilePaused();
  return true;
}

void IoSessionDelegate::SendMessageToFrontend(
    const v8_inspector::StringView& message) {
  io_->Write(TransportAction::kSendMessage, io_->session_id_, message);
}

}  // namespace inspector
}  // namespace node

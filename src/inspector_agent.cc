#include "inspector_agent.h"

#include "inspector_io.h"
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "v8-inspector.h"
#include "v8-platform.h"
#include "util.h"
#include "zlib.h"

#include "libplatform/libplatform.h"

#include <string.h>
#include <vector>

#ifdef __POSIX__
#include <unistd.h>  // setuid, getuid
#endif  // __POSIX__

namespace node {
namespace inspector {
namespace {
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;

using v8_inspector::StringBuffer;
using v8_inspector::StringView;
using v8_inspector::V8Inspector;

static uv_sem_t start_io_thread_semaphore;
static uv_async_t start_io_thread_async;

class StartIoTask : public v8::Task {
 public:
  explicit StartIoTask(Agent* agent) : agent(agent) {}

  void Run() override {
    agent->StartIoThread(false);
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
  static_cast<Agent*>(handle->data)->StartIoThread(false);
}

void StartIoInterrupt(Isolate* isolate, void* agent) {
  static_cast<Agent*>(agent)->StartIoThread(false);
}


#ifdef __POSIX__
static void StartIoThreadWakeup(int signo) {
  uv_sem_post(&start_io_thread_semaphore);
}

inline void* StartIoThreadMain(void* unused) {
  for (;;) {
    uv_sem_wait(&start_io_thread_semaphore);
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
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &sigmask));
  pthread_t thread;
  const int err = pthread_create(&thread, &attr,
                                 StartIoThreadMain, nullptr);
  // Restore original mask
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  CHECK_EQ(0, pthread_attr_destroy(&attr));
  if (err != 0) {
    fprintf(stderr, "node[%d]: pthread_create: %s\n", getpid(), strerror(err));
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

  pid = GetCurrentProcessId();

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

class JsBindingsSessionDelegate : public InspectorSessionDelegate {
 public:
  JsBindingsSessionDelegate(Environment* env,
                            Local<Object> session,
                            Local<Object> receiver,
                            Local<Function> callback)
                            : env_(env),
                              session_(env->isolate(), session),
                              receiver_(env->isolate(), receiver),
                              callback_(env->isolate(), callback) {
    session_.SetWeak(this, JsBindingsSessionDelegate::Release,
                     v8::WeakCallbackType::kParameter);
  }

  ~JsBindingsSessionDelegate() override {
    session_.Reset();
    receiver_.Reset();
    callback_.Reset();
  }

  bool WaitForFrontendMessage() override {
    return false;
  }

  void SendMessageToFrontend(const v8_inspector::StringView& message) override {
    Isolate* isolate = env_->isolate();
    v8::HandleScope handle_scope(isolate);
    Context::Scope context_scope(env_->context());
    MaybeLocal<String> v8string =
        String::NewFromTwoByte(isolate, message.characters16(),
                               NewStringType::kNormal, message.length());
    Local<Value> argument = v8string.ToLocalChecked().As<Value>();
    Local<Function> callback = callback_.Get(isolate);
    Local<Object> receiver = receiver_.Get(isolate);
    static_cast<void>(callback->Call(env_->context(), receiver, 1, &argument));
  }

  void Disconnect() {
    Agent* agent = env_->inspector_agent();
    if (agent->delegate() == this)
      agent->Disconnect();
  }

 private:
  static void Release(
      const v8::WeakCallbackInfo<JsBindingsSessionDelegate>& info) {
    info.SetSecondPassCallback(ReleaseSecondPass);
    info.GetParameter()->session_.Reset();
  }

  static void ReleaseSecondPass(
      const v8::WeakCallbackInfo<JsBindingsSessionDelegate>& info) {
    JsBindingsSessionDelegate* delegate = info.GetParameter();
    delegate->Disconnect();
    delete delegate;
  }

  Environment* env_;
  Persistent<Object> session_;
  Persistent<Object> receiver_;
  Persistent<Function> callback_;
};

void SetDelegate(Environment* env, Local<Object> inspector,
                 JsBindingsSessionDelegate* delegate) {
  inspector->SetPrivate(env->context(),
                        env->inspector_delegate_private_symbol(),
                        v8::External::New(env->isolate(), delegate));
}

Maybe<JsBindingsSessionDelegate*> GetDelegate(
    const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Local<Value> delegate;
  MaybeLocal<Value> maybe_delegate =
      info.This()->GetPrivate(env->context(),
                              env->inspector_delegate_private_symbol());

  if (maybe_delegate.ToLocal(&delegate)) {
    CHECK(delegate->IsExternal());
    void* value = delegate.As<External>()->Value();
    if (value != nullptr) {
      return v8::Just(static_cast<JsBindingsSessionDelegate*>(value));
    }
  }
  env->ThrowError("Inspector is not connected");
  return v8::Nothing<JsBindingsSessionDelegate*>();
}

void Dispatch(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  if (!info[0]->IsString()) {
    env->ThrowError("Inspector message must be a string");
    return;
  }
  Maybe<JsBindingsSessionDelegate*> maybe_delegate = GetDelegate(info);
  if (maybe_delegate.IsNothing())
    return;
  Agent* inspector = env->inspector_agent();
  CHECK_EQ(maybe_delegate.ToChecked(), inspector->delegate());
  inspector->Dispatch(ToProtocolString(env->isolate(), info[0])->string());
}

void Disconnect(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Maybe<JsBindingsSessionDelegate*> delegate = GetDelegate(info);
  if (delegate.IsNothing()) {
    return;
  }
  delegate.ToChecked()->Disconnect();
  SetDelegate(env, info.This(), nullptr);
  delete delegate.ToChecked();
}

void ConnectJSBindingsSession(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  if (!info[0]->IsFunction()) {
    env->ThrowError("Message callback is required");
    return;
  }
  Agent* inspector = env->inspector_agent();
  if (inspector->delegate() != nullptr) {
    env->ThrowError("Session is already attached");
    return;
  }
  Local<Object> session = Object::New(env->isolate());
  env->SetMethod(session, "dispatch", Dispatch);
  env->SetMethod(session, "disconnect", Disconnect);
  info.GetReturnValue().Set(session);

  JsBindingsSessionDelegate* delegate =
      new JsBindingsSessionDelegate(env, session, info.Holder(),
                                    info[0].As<Function>());
  inspector->Connect(delegate);
  SetDelegate(env, session, delegate);
}

void InspectorConsoleCall(const v8::FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  CHECK_LT(2, info.Length());
  std::vector<Local<Value>> call_args;
  for (int i = 3; i < info.Length(); ++i) {
    call_args.push_back(info[i]);
  }
  Environment* env = Environment::GetCurrent(isolate);
  if (env->inspector_agent()->enabled()) {
    Local<Value> inspector_method = info[0];
    CHECK(inspector_method->IsFunction());
    Local<Value> config_value = info[2];
    CHECK(config_value->IsObject());
    Local<Object> config_object = config_value.As<Object>();
    Local<String> in_call_key = FIXED_ONE_BYTE_STRING(isolate, "in_call");
    if (!config_object->Has(context, in_call_key).FromMaybe(false)) {
      CHECK(config_object->Set(context,
                               in_call_key,
                               v8::True(isolate)).FromJust());
      CHECK(!inspector_method.As<Function>()->Call(context,
                                                   info.Holder(),
                                                   call_args.size(),
                                                   call_args.data()).IsEmpty());
    }
    CHECK(config_object->Delete(context, in_call_key).FromJust());
  }

  Local<Value> node_method = info[1];
  CHECK(node_method->IsFunction());
  static_cast<void>(node_method.As<Function>()->Call(context,
                                                     info.Holder(),
                                                     call_args.size(),
                                                     call_args.data()));
}

void CallAndPauseOnStart(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GT(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  std::vector<v8::Local<v8::Value>> call_args;
  for (int i = 2; i < args.Length(); i++) {
    call_args.push_back(args[i]);
  }

  env->inspector_agent()->PauseOnNextJavascriptStatement("Break on start");
  v8::MaybeLocal<v8::Value> retval =
      args[0].As<v8::Function>()->Call(env->context(), args[1],
                                       call_args.size(), call_args.data());
  args.GetReturnValue().Set(retval.ToLocalChecked());
}

// Used in NodeInspectorClient::currentTimeMS() below.
const int NANOS_PER_MSEC = 1000000;
const int CONTEXT_GROUP_ID = 1;

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit ChannelImpl(V8Inspector* inspector,
                       InspectorSessionDelegate* delegate)
                       : delegate_(delegate) {
    session_ = inspector->connect(1, this, StringView());
  }

  virtual ~ChannelImpl() {}

  void dispatchProtocolMessage(const StringView& message) {
    session_->dispatchProtocolMessage(message);
  }

  bool waitForFrontendMessage() {
    return delegate_->WaitForFrontendMessage();
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    std::unique_ptr<StringBuffer> buffer = Utf8ToStringView(reason);
    session_->schedulePauseOnNextStatement(buffer->string(), buffer->string());
  }

  InspectorSessionDelegate* delegate() {
    return delegate_;
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

  InspectorSessionDelegate* const delegate_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
};

}  // namespace

class NodeInspectorClient : public v8_inspector::V8InspectorClient {
 public:
  NodeInspectorClient(node::Environment* env,
                      v8::Platform* platform) : env_(env),
                                                platform_(platform),
                                                terminated_(false),
                                                running_nested_loop_(false) {
    client_ = V8Inspector::create(env->isolate(), this);
  }

  void runMessageLoopOnPause(int context_group_id) override {
    CHECK_NE(channel_, nullptr);
    if (running_nested_loop_)
      return;
    terminated_ = false;
    running_nested_loop_ = true;
    while (!terminated_ && channel_->waitForFrontendMessage()) {
      while (v8::platform::PumpMessageLoop(platform_, env_->isolate()))
        {}
    }
    terminated_ = false;
    running_nested_loop_ = false;
  }

  double currentTimeMS() override {
    return uv_hrtime() * 1.0 / NANOS_PER_MSEC;
  }

  void contextCreated(Local<Context> context, const std::string& name) {
    std::unique_ptr<StringBuffer> name_buffer = Utf8ToStringView(name);
    v8_inspector::V8ContextInfo info(context, CONTEXT_GROUP_ID,
                                     name_buffer->string());
    client_->contextCreated(info);
  }

  void contextDestroyed(Local<Context> context) {
    client_->contextDestroyed(context);
  }

  void quitMessageLoopOnPause() override {
    terminated_ = true;
  }

  void connectFrontend(InspectorSessionDelegate* delegate) {
    CHECK_EQ(channel_, nullptr);
    channel_ = std::unique_ptr<ChannelImpl>(
        new ChannelImpl(client_.get(), delegate));
  }

  void disconnectFrontend() {
    quitMessageLoopOnPause();
    channel_.reset();
  }

  void dispatchMessageFromFrontend(const StringView& message) {
    CHECK_NE(channel_, nullptr);
    channel_->dispatchProtocolMessage(message);
  }

  Local<Context> ensureDefaultContextInGroup(int contextGroupId) override {
    return env_->context();
  }

  void FatalException(Local<Value> error, Local<v8::Message> message) {
    Local<Context> context = env_->context();

    int script_id = message->GetScriptOrigin().ScriptID()->Value();

    Local<v8::StackTrace> stack_trace = message->GetStackTrace();

    if (!stack_trace.IsEmpty() &&
        stack_trace->GetFrameCount() > 0 &&
        script_id == stack_trace->GetFrame(0)->GetScriptId()) {
      script_id = 0;
    }

    const uint8_t DETAILS[] = "Uncaught";

    Isolate* isolate = context->GetIsolate();

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

  ChannelImpl* channel() {
    return channel_.get();
  }

 private:
  node::Environment* env_;
  v8::Platform* platform_;
  bool terminated_;
  bool running_nested_loop_;
  std::unique_ptr<V8Inspector> client_;
  std::unique_ptr<ChannelImpl> channel_;
};

Agent::Agent(Environment* env) : parent_env_(env),
                                 client_(nullptr),
                                 platform_(nullptr),
                                 enabled_(false) {}

// Destructor needs to be defined here in implementation file as the header
// does not have full definition of some classes.
Agent::~Agent() {
}

bool Agent::Start(v8::Platform* platform, const char* path,
                  const DebugOptions& options) {
  path_ = path == nullptr ? "" : path;
  debug_options_ = options;
  client_ =
      std::unique_ptr<NodeInspectorClient>(
          new NodeInspectorClient(parent_env_, platform));
  client_->contextCreated(parent_env_->context(), "Node.js Main Context");
  platform_ = platform;
  CHECK_EQ(0, uv_async_init(uv_default_loop(),
                            &start_io_thread_async,
                            StartIoThreadAsyncCallback));
  start_io_thread_async.data = this;
  uv_unref(reinterpret_cast<uv_handle_t*>(&start_io_thread_async));

  // Ignore failure, SIGUSR1 won't work, but that should not block node start.
  StartDebugSignalHandler();
  if (options.inspector_enabled()) {
    return StartIoThread(options.wait_for_connect());
  }
  return true;
}

bool Agent::StartIoThread(bool wait_for_connect) {
  if (io_ != nullptr)
    return true;

  CHECK_NE(client_, nullptr);

  enabled_ = true;
  io_ = std::unique_ptr<InspectorIo>(
      new InspectorIo(parent_env_, platform_, path_, debug_options_,
                      wait_for_connect));
  if (!io_->Start()) {
    client_.reset();
    return false;
  }

  v8::Isolate* isolate = parent_env_->isolate();

  // Send message to enable debug in workers
  HandleScope handle_scope(isolate);
  Local<Object> process_object = parent_env_->process_object();
  Local<Value> emit_fn =
      process_object->Get(FIXED_ONE_BYTE_STRING(isolate, "emit"));
  // In case the thread started early during the startup
  if (!emit_fn->IsFunction())
    return true;

  Local<Object> message = Object::New(isolate);
  message->Set(FIXED_ONE_BYTE_STRING(isolate, "cmd"),
               FIXED_ONE_BYTE_STRING(isolate, "NODE_DEBUG_ENABLED"));
  Local<Value> argv[] = {
    FIXED_ONE_BYTE_STRING(isolate, "internalMessage"),
    message
  };
  MakeCallback(parent_env_->isolate(), process_object, emit_fn.As<Function>(),
               arraysize(argv), argv, 0, 0);

  return true;
}

void Agent::Stop() {
  if (io_ != nullptr) {
    io_->Stop();
    io_.reset();
  }
}

void Agent::Connect(InspectorSessionDelegate* delegate) {
  enabled_ = true;
  client_->connectFrontend(delegate);
}

bool Agent::IsConnected() {
  return io_ && io_->IsConnected();
}

void Agent::WaitForDisconnect() {
  CHECK_NE(client_, nullptr);
  client_->contextDestroyed(parent_env_->context());
  if (io_ != nullptr) {
    io_->WaitForDisconnect();
  }
}

void Agent::FatalException(Local<Value> error, Local<v8::Message> message) {
  if (!IsStarted())
    return;
  client_->FatalException(error, message);
  WaitForDisconnect();
}

void Agent::Dispatch(const StringView& message) {
  CHECK_NE(client_, nullptr);
  client_->dispatchMessageFromFrontend(message);
}

void Agent::Disconnect() {
  CHECK_NE(client_, nullptr);
  client_->disconnectFrontend();
}

void Agent::RunMessageLoop() {
  CHECK_NE(client_, nullptr);
  client_->runMessageLoopOnPause(CONTEXT_GROUP_ID);
}

InspectorSessionDelegate* Agent::delegate() {
  CHECK_NE(client_, nullptr);
  ChannelImpl* channel = client_->channel();
  if (channel == nullptr)
    return nullptr;
  return channel->delegate();
}

void Agent::PauseOnNextJavascriptStatement(const std::string& reason) {
  ChannelImpl* channel = client_->channel();
  if (channel != nullptr)
    channel->schedulePauseOnNextStatement(reason);
}

// static
void Agent::InitInspector(Local<Object> target, Local<Value> unused,
                          Local<Context> context, void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Agent* agent = env->inspector_agent();
  env->SetMethod(target, "consoleCall", InspectorConsoleCall);
  if (agent->debug_options_.wait_for_connect())
    env->SetMethod(target, "callAndPauseOnStart", CallAndPauseOnStart);
  env->SetMethod(target, "connect", ConnectJSBindingsSession);
}

void Agent::RequestIoThreadStart() {
  // We need to attempt to interrupt V8 flow (in case Node is running
  // continuous JS code) and to wake up libuv thread (in case Node is wating
  // for IO events)
  uv_async_send(&start_io_thread_async);
  v8::Isolate* isolate = parent_env_->isolate();
  platform_->CallOnForegroundThread(isolate, new StartIoTask(this));
  isolate->RequestInterrupt(StartIoInterrupt, this);
  uv_async_send(&start_io_thread_async);
}

}  // namespace inspector
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(inspector,
                                  node::inspector::Agent::InitInspector);

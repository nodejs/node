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
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

using v8_inspector::StringBuffer;
using v8_inspector::StringView;
using v8_inspector::V8Inspector;

static uv_sem_t inspector_io_thread_semaphore;
static uv_async_t start_inspector_thread_async;

std::unique_ptr<StringBuffer> ToProtocolString(Isolate* isolate,
                                               Local<Value> value) {
  TwoByteValue buffer(isolate, value);
  return StringBuffer::create(StringView(*buffer, buffer.length()));
}

// Called from the main thread.
void StartInspectorIoThreadAsyncCallback(uv_async_t* handle) {
  static_cast<Agent*>(handle->data)->StartIoThread();
}

#ifdef __POSIX__
static void EnableInspectorIOThreadSignalHandler(int signo) {
  uv_sem_post(&inspector_io_thread_semaphore);
}

inline void* InspectorIoThreadSignalThreadMain(void* unused) {
  for (;;) {
    uv_sem_wait(&inspector_io_thread_semaphore);
    uv_async_send(&start_inspector_thread_async);
  }
  return nullptr;
}

static int RegisterDebugSignalHandler() {
  // Start a watchdog thread for calling v8::Debug::DebugBreak() because
  // it's not safe to call directly from the signal handler, it can
  // deadlock with the thread it interrupts.
  CHECK_EQ(0, uv_sem_init(&inspector_io_thread_semaphore, 0));
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
  sigfillset(&sigmask);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &sigmask));
  pthread_t thread;
  const int err = pthread_create(&thread, &attr,
                                 InspectorIoThreadSignalThreadMain, nullptr);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  CHECK_EQ(0, pthread_attr_destroy(&attr));
  if (err != 0) {
    fprintf(stderr, "node[%d]: pthread_create: %s\n", getpid(), strerror(err));
    fflush(stderr);
    // Leave SIGUSR1 blocked.  We don't install a signal handler,
    // receiving the signal would terminate the process.
    return -err;
  }
  RegisterSignalHandler(SIGUSR1, EnableInspectorIOThreadSignalHandler);
  // Unblock SIGUSR1.  A pending SIGUSR1 signal will now be delivered.
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr));
  return 0;
}
#endif  // __POSIX__


#ifdef _WIN32
DWORD WINAPI EnableDebugThreadProc(void* arg) {
  uv_async_send(&start_inspector_thread_async);
  return 0;
}

static int GetDebugSignalHandlerMappingName(DWORD pid, wchar_t* buf,
                                            size_t buf_len) {
  return _snwprintf(buf, buf_len, L"node-debug-handler-%u", pid);
}

static int RegisterDebugSignalHandler() {
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

  *handler = EnableDebugThreadProc;

  UnmapViewOfFile(static_cast<void*>(handler));

  return 0;
}
#endif  // _WIN32
}  // namespace


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
    delegate_->OnMessage(message);
  }

  InspectorSessionDelegate* const delegate_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
};

class NodeInspectorClient : public v8_inspector::V8InspectorClient {
 public:
  NodeInspectorClient(node::Environment* env,
                      v8::Platform* platform) : env_(env),
                                                platform_(platform),
                                                terminated_(false),
                                                running_nested_loop_(false) {
    inspector_ = V8Inspector::create(env->isolate(), this);
    const uint8_t CONTEXT_NAME[] = "Node.js Main Context";
    StringView context_name(CONTEXT_NAME, sizeof(CONTEXT_NAME) - 1);
    v8_inspector::V8ContextInfo info(env->context(), CONTEXT_GROUP_ID,
                                     context_name);
    inspector_->contextCreated(info);
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

  void quitMessageLoopOnPause() override {
    terminated_ = true;
  }

  void connectFrontend(InspectorSessionDelegate* delegate) {
    CHECK_EQ(channel_, nullptr);
    channel_ = std::unique_ptr<ChannelImpl>(
        new ChannelImpl(inspector_.get(), delegate));
  }

  void disconnectFrontend() {
    quitMessageLoopOnPause();
    channel_.reset();
  }

  void dispatchMessageFromFrontend(const StringView& message) {
    CHECK_NE(channel_, nullptr);
    channel_->dispatchProtocolMessage(message);
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    if (channel_ != nullptr) {
      channel_->schedulePauseOnNextStatement(reason);
    }
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

    inspector_->exceptionThrown(
        context,
        StringView(DETAILS, sizeof(DETAILS) - 1),
        error,
        ToProtocolString(isolate, message->Get())->string(),
        ToProtocolString(isolate, message->GetScriptResourceName())->string(),
        message->GetLineNumber(context).FromMaybe(0),
        message->GetStartColumn(context).FromMaybe(0),
        inspector_->createStackTrace(stack_trace),
        script_id);
  }

  InspectorSessionDelegate* delegate() {
    if (channel_ == nullptr)
      return nullptr;
    return channel_->delegate();
  }

 private:
  node::Environment* env_;
  v8::Platform* platform_;
  bool terminated_;
  bool running_nested_loop_;
  std::unique_ptr<V8Inspector> inspector_;
  std::unique_ptr<ChannelImpl> channel_;
};

Agent::Agent(Environment* env) : parent_env_(env),
                                 inspector_(nullptr),
                                 platform_(nullptr),
                                 inspector_console_(false) {}

// Header has unique_ptr to some incomplete types - this definition tells
// the compiler to figure out destruction here, were those types are complete
Agent::~Agent() {
}

// static
void Agent::InspectorConsoleCall(const v8::FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  CHECK(info.Data()->IsArray());
  Local<v8::Array> args = info.Data().As<v8::Array>();
  CHECK_EQ(args->Length(), 3);

  std::vector<Local<Value>> call_args(info.Length());
  for (int i = 0; i < info.Length(); ++i) {
    call_args[i] = info[i];
  }

  Environment* env = Environment::GetCurrent(isolate);
  if (env->inspector_agent()->inspector_console_) {
    Local<Value> inspector_method = args->Get(context, 0).ToLocalChecked();
    CHECK(inspector_method->IsFunction());
    Local<Value> config_value = args->Get(context, 2).ToLocalChecked();
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

  Local<Value> node_method =
      args->Get(context, 1).ToLocalChecked();
  CHECK(node_method->IsFunction());
  static_cast<void>(node_method.As<Function>()->Call(context,
                                                     info.Holder(),
                                                     call_args.size(),
                                                     call_args.data()));
}

// static
void Agent::InspectorWrapConsoleCall(const FunctionCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  if (info.Length() != 3 || !info[0]->IsFunction() ||
      !info[1]->IsFunction() || !info[2]->IsObject()) {
    return env->ThrowError("inspector.wrapConsoleCall takes exactly 3 "
        "arguments: two functions and an object.");
  }

  Local<v8::Array> array = v8::Array::New(env->isolate(), info.Length());
  CHECK(array->Set(env->context(), 0, info[0]).FromJust());
  CHECK(array->Set(env->context(), 1, info[1]).FromJust());
  CHECK(array->Set(env->context(), 2, info[2]).FromJust());
  info.GetReturnValue().Set(Function::New(env->context(),
                                          InspectorConsoleCall,
                                          array).ToLocalChecked());
}

bool Agent::Start(v8::Platform* platform, const char* path,
                  const DebugOptions& options) {
  path_ = path == nullptr ? "" : path;
  debug_options_ = options;
  inspector_console_ = false;
  inspector_ =
      std::unique_ptr<NodeInspectorClient>(
          new NodeInspectorClient(parent_env_, platform));
  platform_ = platform;
  Local<Object> process = parent_env_->process_object();
  Local<Object> inspector = Object::New(parent_env_->isolate());
  Local<String> name =
      FIXED_ONE_BYTE_STRING(parent_env_->isolate(), "inspector");
  process->DefineOwnProperty(parent_env_->context(),
                             name,
                             inspector,
                             v8::ReadOnly).FromJust();
  parent_env_->SetMethod(inspector, "wrapConsoleCall",
                         InspectorWrapConsoleCall);
  if (options.inspector_enabled()) {
    if (options.wait_for_connect()) {
      parent_env_->SetMethod(inspector, "callAndPauseOnStart",
                             CallAndPauseOnStart);
    }
    return StartIoThread();
  } else {
    CHECK_EQ(0, uv_async_init(uv_default_loop(),
                              &start_inspector_thread_async,
                              StartInspectorIoThreadAsyncCallback));
    start_inspector_thread_async.data = this;
    uv_unref(reinterpret_cast<uv_handle_t*>(&start_inspector_thread_async));

    RegisterDebugSignalHandler();
    return true;
  }
}

bool Agent::StartIoThread() {
  if (io_ != nullptr)
    return true;

  CHECK_NE(inspector_, nullptr);

  inspector_console_ = true;
  io_ = std::unique_ptr<InspectorIo>(
      new InspectorIo(parent_env_, platform_, path_, debug_options_));
  if (!io_->Start()) {
    inspector_.reset();
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
  MakeCallback(parent_env_, process_object.As<Value>(), emit_fn.As<Function>(),
               arraysize(argv), argv);

  return true;
}

void Agent::Stop() {
  if (io_ != nullptr)
    io_->Stop();
}

void Agent::Connect(InspectorSessionDelegate* delegate) {
  inspector_console_ = true;
  inspector_->connectFrontend(delegate);
}

bool Agent::IsConnected() {
  return io_ && io_->IsConnected();
}

bool Agent::IsStarted() {
  return !!inspector_;
}

// static
void Agent::CallAndPauseOnStart(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GT(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  std::vector<v8::Local<v8::Value>> call_args;
  for (int i = 2; i < args.Length(); i++) {
    call_args.push_back(args[i]);
  }

  Agent* agent = env->inspector_agent();
  agent->inspector_->schedulePauseOnNextStatement("Break on start");

  v8::MaybeLocal<v8::Value> retval =
      args[0].As<v8::Function>()->Call(env->context(), args[1],
                                       call_args.size(), call_args.data());
  args.GetReturnValue().Set(retval.ToLocalChecked());
}

void Agent::WaitForDisconnect() {
  if (io_ != nullptr) {
    io_->WaitForDisconnect();
  }
}

void Agent::FatalException(Local<Value> error, Local<v8::Message> message) {
  if (!IsStarted())
    return;
  inspector_->FatalException(error, message);
  WaitForDisconnect();
}

void Agent::Dispatch(const StringView& message) {
  CHECK_NE(inspector_, nullptr);
  inspector_->dispatchMessageFromFrontend(message);
}

void Agent::Disconnect() {
  CHECK_NE(inspector_, nullptr);
  inspector_->disconnectFrontend();
}

void Agent::RunMessageLoop() {
  CHECK_NE(inspector_, nullptr);
  inspector_->runMessageLoopOnPause(CONTEXT_GROUP_ID);
}

}  // namespace inspector
}  // namespace node

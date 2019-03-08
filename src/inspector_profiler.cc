#include "base_object-inl.h"
#include "debug_utils.h"
#include "inspector_agent.h"
#include "node_internals.h"
#include "v8-inspector.h"

namespace node {
namespace profiler {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

using v8_inspector::StringBuffer;
using v8_inspector::StringView;

std::unique_ptr<StringBuffer> ToProtocolString(Isolate* isolate,
                                               Local<Value> value) {
  TwoByteValue buffer(isolate, value);
  return StringBuffer::create(StringView(*buffer, buffer.length()));
}

class V8ProfilerConnection : public BaseObject {
 public:
  class V8ProfilerSessionDelegate
      : public inspector::InspectorSessionDelegate {
   public:
    explicit V8ProfilerSessionDelegate(V8ProfilerConnection* connection)
        : connection_(connection) {}

    void SendMessageToFrontend(
        const v8_inspector::StringView& message) override {
      Environment* env = connection_->env();

      Local<Function> fn = connection_->GetMessageCallback();
      bool ending = !fn.IsEmpty();
      Debug(env,
            DebugCategory::INSPECTOR_PROFILER,
            "Sending message to frontend, ending = %s\n",
            ending ? "true" : "false");
      if (!ending) {
        return;
      }
      Isolate* isolate = env->isolate();

      HandleScope handle_scope(isolate);
      Context::Scope context_scope(env->context());
      MaybeLocal<String> v8string =
          String::NewFromTwoByte(isolate,
                                 message.characters16(),
                                 NewStringType::kNormal,
                                 message.length());
      Local<Value> args[] = {v8string.ToLocalChecked().As<Value>()};
      USE(fn->Call(
          env->context(), connection_->object(), arraysize(args), args));
    }

   private:
    V8ProfilerConnection* connection_;
  };

  SET_MEMORY_INFO_NAME(V8ProfilerConnection)
  SET_SELF_SIZE(V8ProfilerConnection)

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize(
        "session", sizeof(*session_), "InspectorSession");
  }

  explicit V8ProfilerConnection(Environment* env, Local<Object> obj)
      : BaseObject(env, obj), session_(nullptr) {
    inspector::Agent* inspector = env->inspector_agent();
    std::unique_ptr<inspector::InspectorSession> session = inspector->Connect(
        std::make_unique<V8ProfilerSessionDelegate>(this), false);
    session_ = std::move(session);
    MakeWeak();
  }

  void DispatchMessage(Isolate* isolate, Local<String> message) {
    session_->Dispatch(ToProtocolString(isolate, message)->string());
  }

  static MaybeLocal<Object> CreateConnectionObject(Environment* env) {
    Isolate* isolate = env->isolate();
    Local<Context> context = env->context();
    EscapableHandleScope scope(isolate);

    Local<ObjectTemplate> t = ObjectTemplate::New(isolate);
    t->SetInternalFieldCount(1);
    Local<Object> obj;
    if (!t->NewInstance(context).ToLocal(&obj)) {
      return MaybeLocal<Object>();
    }

    obj->SetAlignedPointerInInternalField(0, nullptr);
    return scope.Escape(obj);
  }

  void Start() {
    SetConnection(object());
    StartProfiling();
  }

  void End(Local<Function> callback) {
    SetMessageCallback(callback);
    EndProfiling();
  }

  // Override this to return a JS function that gets called with the message
  // sent from the session.
  virtual Local<Function> GetMessageCallback() = 0;
  virtual void SetMessageCallback(Local<Function> callback) = 0;
  // Use DispatchMessage() to dispatch necessary inspector messages
  virtual void StartProfiling() = 0;
  virtual void EndProfiling() = 0;
  virtual void SetConnection(Local<Object> connection) = 0;

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
};

class V8CoverageConnection : public V8ProfilerConnection {
 public:
  explicit V8CoverageConnection(Environment* env)
      : V8ProfilerConnection(env,
                             CreateConnectionObject(env).ToLocalChecked()) {}

  Local<Function> GetMessageCallback() override {
    return env()->on_coverage_message_function();
  }

  void SetMessageCallback(Local<Function> callback) override {
    return env()->set_on_coverage_message_function(callback);
  }

  static V8ProfilerConnection* GetConnection(Environment* env) {
    return Unwrap<V8CoverageConnection>(env->coverage_connection());
  }

  void SetConnection(Local<Object> obj) override {
    env()->set_coverage_connection(obj);
  }

  void StartProfiling() override {
    Debug(env(),
          DebugCategory::INSPECTOR_PROFILER,
          "Sending Profiler.startPreciseCoverage\n");
    Isolate* isolate = env()->isolate();
    Local<String> enable = FIXED_ONE_BYTE_STRING(
        isolate, "{\"id\": 1, \"method\": \"Profiler.enable\"}");
    Local<String> start = FIXED_ONE_BYTE_STRING(
        isolate,
        "{"
        "\"id\": 2,"
        "\"method\": \"Profiler.startPreciseCoverage\","
        "\"params\": {\"callCount\": true, \"detailed\": true}"
        "}");
    DispatchMessage(isolate, enable);
    DispatchMessage(isolate, start);
  }

  void EndProfiling() override {
    Debug(env(),
          DebugCategory::INSPECTOR_PROFILER,
          "Sending Profiler.takePreciseCoverage\n");
    Isolate* isolate = env()->isolate();
    Local<String> end =
        FIXED_ONE_BYTE_STRING(isolate,
                              "{"
                              "\"id\": 3,"
                              "\"method\": \"Profiler.takePreciseCoverage\""
                              "}");
    DispatchMessage(isolate, end);
  }

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
};

void StartCoverageCollection(Environment* env) {
  V8CoverageConnection* connection = new V8CoverageConnection(env);
  connection->Start();
}

static void EndCoverageCollection(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "Ending coverage collection\n");
  V8ProfilerConnection* connection = V8CoverageConnection::GetConnection(env);
  CHECK_NOT_NULL(connection);
  connection->End(args[0].As<Function>());
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "endCoverage", EndCoverageCollection);
}
}  // namespace profiler
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(profiler, node::profiler::Initialize)

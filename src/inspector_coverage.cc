#include "base_object-inl.h"
#include "debug_utils.h"
#include "inspector_agent.h"
#include "node_internals.h"
#include "v8-inspector.h"

namespace node {
namespace coverage {

using v8::Context;
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

class V8CoverageConnection : public BaseObject {
 public:
  class V8CoverageSessionDelegate : public inspector::InspectorSessionDelegate {
   public:
    explicit V8CoverageSessionDelegate(V8CoverageConnection* connection)
        : connection_(connection) {}

    void SendMessageToFrontend(
        const v8_inspector::StringView& message) override {
      Environment* env = connection_->env();
      Local<Function> fn = connection_->env()->on_coverage_message_function();
      bool ending = !fn.IsEmpty();
      Debug(env,
            DebugCategory::COVERAGE,
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
      USE(MakeCallback(isolate,
                       connection_->object(),
                       fn,
                       arraysize(args),
                       args,
                       async_context{0, 0}));
    }

   private:
    V8CoverageConnection* connection_;
  };

  SET_MEMORY_INFO_NAME(V8CoverageConnection)
  SET_SELF_SIZE(V8CoverageConnection)

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize(
        "session", sizeof(*session_), "InspectorSession");
  }

  explicit V8CoverageConnection(Environment* env)
      : BaseObject(env, env->coverage_connection()), session_(nullptr) {
    inspector::Agent* inspector = env->inspector_agent();
    std::unique_ptr<inspector::InspectorSession> session = inspector->Connect(
        std::make_unique<V8CoverageSessionDelegate>(this), false);
    session_ = std::move(session);
    MakeWeak();
  }

  void Start() {
    Debug(this->env(),
          DebugCategory::COVERAGE,
          "Sending Profiler.startPreciseCoverage\n");
    Isolate* isolate = this->env()->isolate();
    Local<Value> enable = FIXED_ONE_BYTE_STRING(
        isolate, "{\"id\": 1, \"method\": \"Profiler.enable\"}");
    Local<Value> start = FIXED_ONE_BYTE_STRING(
        isolate,
        "{"
        "\"id\": 2,"
        "\"method\": \"Profiler.startPreciseCoverage\","
        "\"params\": {\"callCount\": true, \"detailed\": true}"
        "}");
    session_->Dispatch(ToProtocolString(isolate, enable)->string());
    session_->Dispatch(ToProtocolString(isolate, start)->string());
  }

  void End() {
    Debug(this->env(),
          DebugCategory::COVERAGE,
          "Sending Profiler.takePreciseCoverage\n");
    Isolate* isolate = this->env()->isolate();
    Local<Value> end =
        FIXED_ONE_BYTE_STRING(isolate,
                              "{"
                              "\"id\": 3,"
                              "\"method\": \"Profiler.takePreciseCoverage\""
                              "}");
    session_->Dispatch(ToProtocolString(isolate, end)->string());
  }

  friend class V8CoverageSessionDelegate;

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
};

bool StartCoverageCollection(Environment* env) {
  HandleScope scope(env->isolate());

  Local<ObjectTemplate> t = ObjectTemplate::New(env->isolate());
  t->SetInternalFieldCount(1);
  Local<Object> obj;
  if (!t->NewInstance(env->context()).ToLocal(&obj)) {
    return false;
  }

  obj->SetAlignedPointerInInternalField(0, nullptr);

  CHECK(env->coverage_connection().IsEmpty());
  env->set_coverage_connection(obj);
  V8CoverageConnection* connection = new V8CoverageConnection(env);
  connection->Start();
  return true;
}

static void EndCoverageCollection(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  Debug(env, DebugCategory::COVERAGE, "Ending coverage collection\n");
  env->set_on_coverage_message_function(args[0].As<Function>());
  V8CoverageConnection* connection =
      Unwrap<V8CoverageConnection>(env->coverage_connection());
  CHECK_NOT_NULL(connection);
  connection->End();
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "end", EndCoverageCollection);
}
}  // namespace coverage
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(coverage, node::coverage::Initialize)

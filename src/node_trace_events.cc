#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_v8_platform-inl.h"
#include "util-inl.h"

#ifndef V8_USE_PERFETTO
#include "tracing/agent.h"
#else
#include "node_trace_events.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "handle_wrap.h"
#include "tracing/tracing.h"
#include "node_v8_platform-inl.h"
#endif

#include <set>
#include <string>

namespace node {

class ExternalReferenceRegistry;

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

#ifndef V8_USE_PERFETTO

class NodeCategorySet : public BaseObject {
 public:
  static void Initialize(Local<Object> target,
                  Local<Value> unused,
                  Local<Context> context,
                  void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Enable(const FunctionCallbackInfo<Value>& args);
  static void Disable(const FunctionCallbackInfo<Value>& args);

  const std::set<std::string>& GetCategories() const { return categories_; }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("categories", categories_);
  }

  SET_MEMORY_INFO_NAME(NodeCategorySet)
  SET_SELF_SIZE(NodeCategorySet)

 private:
  NodeCategorySet(Environment* env,
                  Local<Object> wrap,
                  std::set<std::string>&& categories) :
        BaseObject(env, wrap), categories_(std::move(categories)) {
    MakeWeak();
  }

  bool enabled_ = false;
  const std::set<std::string> categories_;
};

void NodeCategorySet::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::set<std::string> categories;
  CHECK(args[0]->IsArray());
  Local<Array> cats = args[0].As<Array>();
  for (size_t n = 0; n < cats->Length(); n++) {
    Local<Value> category;
    if (!cats->Get(env->context(), n).ToLocal(&category)) return;
    Utf8Value val(env->isolate(), category);
    if (!*val) return;
    categories.emplace(*val);
  }
  CHECK_NOT_NULL(GetTracingAgentWriter());
  new NodeCategorySet(env, args.This(), std::move(categories));
}

void NodeCategorySet::Enable(const FunctionCallbackInfo<Value>& args) {
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (!category_set->enabled_ && !categories.empty()) {
    // Starts the Tracing Agent if it wasn't started already (e.g. through
    // a command line flag.)
    StartTracingAgent();
    GetTracingAgentWriter()->Enable(categories);
    category_set->enabled_ = true;
  }
}

void NodeCategorySet::Disable(const FunctionCallbackInfo<Value>& args) {
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (category_set->enabled_ && !categories.empty()) {
    GetTracingAgentWriter()->Disable(categories);
    category_set->enabled_ = false;
  }
}

void GetEnabledCategories(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::string categories =
      GetTracingAgentWriter()->agent()->GetEnabledCategories();
  if (!categories.empty()) {
    args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          categories.c_str(),
                          NewStringType::kNormal,
                          categories.size()).ToLocalChecked());
  }
}

static void SetTraceCategoryStateUpdateHandler(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_trace_category_state_function(args[0].As<Function>());
}

void NodeCategorySet::Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "getEnabledCategories", GetEnabledCategories);
  env->SetMethod(
      target, "setTraceCategoryStateUpdateHandler",
      SetTraceCategoryStateUpdateHandler);

  Local<FunctionTemplate> category_set =
      env->NewFunctionTemplate(NodeCategorySet::New);
  category_set->InstanceTemplate()->SetInternalFieldCount(
      NodeCategorySet::kInternalFieldCount);
  category_set->Inherit(BaseObject::GetConstructorTemplate(env));
  env->SetProtoMethod(category_set, "enable", NodeCategorySet::Enable);
  env->SetProtoMethod(category_set, "disable", NodeCategorySet::Disable);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "CategorySet"),
              category_set->GetFunction(env->context()).ToLocalChecked())
              .Check();

  Local<String> isTraceCategoryEnabled =
      FIXED_ONE_BYTE_STRING(env->isolate(), "isTraceCategoryEnabled");
  Local<String> trace = FIXED_ONE_BYTE_STRING(env->isolate(), "trace");

  // Grab the trace and isTraceCategoryEnabled intrinsics from the binding
  // object and expose those to our binding layer.
  Local<Object> binding = context->GetExtrasBindingObject();
  target->Set(context, isTraceCategoryEnabled,
              binding->Get(context, isTraceCategoryEnabled).ToLocalChecked())
                  .Check();
  target->Set(context, trace,
              binding->Get(context, trace).ToLocalChecked()).Check();
}

void NodeCategorySet::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetEnabledCategories);
  registry->Register(SetTraceCategoryStateUpdateHandler);
  registry->Register(NodeCategorySet::New);
  registry->Register(NodeCategorySet::Enable);
  registry->Register(NodeCategorySet::Disable);
}

#else

namespace tracing {

void JSTracingSessionTraits::Init(
    JSTracingSessionBase* base,
    const JSTracingSessionBase::Options& options,
    const perfetto::TraceConfig& config) {
  base->session()->Setup(config);
}

void JSTracingSessionTraits::OnStartTracing(
    JSTracingSessionBase* base) {
  JSTracingSession* session = static_cast<JSTracingSession*>(base);
  session->Start();
}

void JSTracingSessionTraits::OnStopTracing(
    JSTracingSessionBase* base) {
  JSTracingSession* session = static_cast<JSTracingSession*>(base);
  session->Stop();
}

void JSTracingSession::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsFunction());  // The callback function
  CHECK(args[1]->IsString());  // categories list
  CHECK_IMPLIES(!args[2]->IsUndefined(), args[2]->IsUint32());  // duration
  CHECK_IMPLIES(!args[3]->IsUndefined(), args[3]->IsUint32());  // flush period

  Environment* env = Environment::GetCurrent(args);
  JSTracingSessionBase::Options options;
  Utf8Value categories(env->isolate(), args[1]);

  // If duration is undefined or zero, no duration will be applied.
  // Otherwise, duration is the number of milliseconds that the
  // tracing session will run for.
  options.duration = args[2].As<Uint32>()->Value();
  options.flush_period = args[3].As<Uint32>()->Value();

  options.enabled_categories = SplitString(*categories, ',');
  new JSTracingSession(
      env,
      args.This(),
      &per_process::v8_platform.tracing_service_,
      options,
      args[0].As<Function>());
}

JSTracingSession::JSTracingSession(
    Environment* env,
    Local<Object> object,
    TracingService* service,
    const JSTracingSessionBase::Options& options,
    Local<Function> callback)
    : HandleWrap(
          env,
          object,
          reinterpret_cast<uv_handle_t*>(&check_),
          AsyncWrap::PROVIDER_TRACINGSESSION),
      JSTracingSessionBase(service, options),
      callback_(env->isolate(), callback) {
  CHECK_EQ(0, uv_check_init(env->event_loop(), &check_));
  uv_unref(GetHandle());
  MarkAsInitialized();
  // Start tracing in case it hasn't been started yet.
  // Will return false if tracing was already started.
  if (!service->StartTracing()) {
    // If the TracingService was not already started, then
    // this session will have been started automatically. If,
    // however, it had already been started, we need to start
    // manually.
    OnStartTracing();
  }
}

void JSTracingSession::Start() {
  uv_check_start(&check_, Handle);
}

void JSTracingSession::Stop() {
  set_finished_state(FinishState::DONE);
}

void JSTracingSession::OnClose() {
  OnStopTracing();
  set_finished_state(FinishState::COMPLETE);
  ProcessData();
}

void JSTracingSession::Handle(uv_check_t* handle) {
  JSTracingSession* session = ContainerOf(&JSTracingSession::check_, handle);
  session->ProcessData();
}

void JSTracingSession::ProcessData() {
  std::vector<char> data = session()->ReadTraceBlocking();
  FinishState finish_state = get_finished_state();
  HandleScope scope(env()->isolate());
  if (data.size() > 0) {
    if (finish_state == FinishState::WAITING)
      set_finished_state(FinishState::COMPLETE);
    AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env(), data.size());
    memcpy(buf.data(), data.data(), data.size());
    Local<Value> arg = buf.ToArrayBuffer();
    MakeCallback(callback_.Get(env()->isolate()), 1, &arg);
    if (finish_state != FinishState::COMPLETE)
      return;
  }

  switch (finish_state) {
    case FinishState::DONE:
      set_finished_state(FinishState::WAITING);
      // Fall through
    case FinishState::WAITING:
      // Fall through
    case FinishState::RUNNING:
      return;
    case FinishState::COMPLETE:
      Local<Value> arg = v8::Null(env()->isolate());
      MakeCallback(callback_.Get(env()->isolate()), 1, &arg);
      uv_check_stop(&check_);
  }
}

namespace {
void GetSharedCategoryArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::unique_ptr<v8::BackingStore> bs =
      ArrayBuffer::NewBackingStore(
          per_process::v8_platform.tracing_service_.shared_category_state(),
          sizeof(TracingService::SharedCategoryState),
          [](void* data, size_t length, void* ptr) {
            // Do nothing. We don't own this state.
          },
          nullptr);
  args.GetReturnValue().Set(ArrayBuffer::New(env->isolate(), std::move(bs)));
}

void GetDynamicCategoryEnabled(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  Utf8Value name(env->isolate(), args[0]);
  perfetto::DynamicCategory category(*name);
  args.GetReturnValue().Set(
      TRACE_EVENT_CATEGORY_ENABLED(category)
          ? v8::True(env->isolate())
          : v8::False(env->isolate()));
}

static void SetTraceCategoryStateUpdateHandler(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_trace_category_state_function(args[0].As<Function>());
}

void AsyncHooksTraceInit(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());  // name
  CHECK(args[1]->IsNumber());  // async_id
  CHECK(args[2]->IsNumber());  // trigger_async_id
  Environment* env = Environment::GetCurrent(args);
  Utf8Value name(env->isolate(), args[0]);
  TRACE_EVENT_BEGIN(
      "node.async_hooks",
      nullptr,
      perfetto::Track(args[1].As<Number>()->Value()),
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_name(*name);
        auto info = ctx.event()->set_async_wrap_info();
        info->set_triggerasyncid(
            static_cast<uint64_t>(args[2].As<Number>()->Value()));
        info->set_executionasyncid(
            static_cast<uint64_t>(env->execution_async_id()));
      });
}

void AsyncHooksTraceDestroy(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsNumber());  // async_id
  TRACE_EVENT_END(
      "node.async_hooks",
      perfetto::Track(args[0].As<Number>()->Value()));
}

void AsyncHooksTraceBefore(const FunctionCallbackInfo<Value> & args) {
  CHECK(args[0]->IsString());  // name
  CHECK(args[1]->IsNumber());  // async_id
  Environment* env = Environment::GetCurrent(args);
  Utf8Value name(env->isolate(), args[0]);
  TRACE_EVENT_BEGIN(
      "node.async_hooks",
      nullptr,
      perfetto::Track(args[1].As<Number>()->Value()),
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_name(std::string(*name) + "_CALLBACK");
      });
}

void AsyncHooksTraceAfter(const FunctionCallbackInfo<Value> & args) {
  CHECK(args[0]->IsNumber());  // async_id
  TRACE_EVENT_END(
      "node.async_hooks",
      perfetto::Track(args[0].As<Number>()->Value()));
}
}  // namespace

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(JSTracingSession::New);
  registry->Register(GetSharedCategoryArrayBuffer);
  registry->Register(GetDynamicCategoryEnabled);
  registry->Register(SetTraceCategoryStateUpdateHandler);
  registry->Register(AsyncHooksTraceInit);
  registry->Register(AsyncHooksTraceDestroy);
  registry->Register(AsyncHooksTraceBefore);
  registry->Register(AsyncHooksTraceAfter);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(
      target,
      "getSharedCategoryArrayBuffer",
      GetSharedCategoryArrayBuffer);
  env->SetMethodNoSideEffect(
      target,
      "getDynamicCategoryEnabled",
      GetDynamicCategoryEnabled);
  env->SetMethod(
      target, "setTraceCategoryStateUpdateHandler",
      SetTraceCategoryStateUpdateHandler);

  env->SetMethod(target, "asyncHooksTraceInit", AsyncHooksTraceInit);
  env->SetMethod(target, "asyncHooksTraceDestroy", AsyncHooksTraceDestroy);
  env->SetMethod(target, "asyncHooksTraceBefore", AsyncHooksTraceBefore);
  env->SetMethod(target, "asyncHooksTraceAfter", AsyncHooksTraceAfter);


  Local<String> class_name =
      FIXED_ONE_BYTE_STRING(env->isolate(), "TracingSession");
  Local<FunctionTemplate> tracing_session =
      env->NewFunctionTemplate(JSTracingSession::New);
  tracing_session->SetClassName(class_name);
  tracing_session->InstanceTemplate()->SetInternalFieldCount(
      JSTracingSession::kInternalFieldCount);
  tracing_session->Inherit(HandleWrap::GetConstructorTemplate(env));

  target->Set(env->context(),
              class_name,
              tracing_session->GetFunction(env->context()).ToLocalChecked())
              .Check();
}

}  // namespace tracing

#endif

}  // namespace node

#ifndef V8_USE_PERFETTO
NODE_MODULE_CONTEXT_AWARE_INTERNAL(trace_events,
                                   node::NodeCategorySet::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(
    trace_events, node::NodeCategorySet::RegisterExternalReferences)
#else
NODE_MODULE_CONTEXT_AWARE_INTERNAL(trace_events,
                                   node::tracing::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(
    trace_events, node::tracing::RegisterExternalReferences)
#endif

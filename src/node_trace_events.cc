#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_internals.h"
#include "node_v8_platform-inl.h"
#include "tracing/agent.h"
#include "util-inl.h"

#include <set>
#include <string>

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

class NodeCategorySet : public BaseObject {
 public:
  static void Initialize(Local<Object> target,
                  Local<Value> unused,
                  Local<Context> context,
                  void* priv);

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

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(trace_events,
                                   node::NodeCategorySet::Initialize)

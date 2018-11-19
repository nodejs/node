#include "node.h"
#include "node_internals.h"
#include "tracing/agent.h"
#include "env.h"
#include "base_object-inl.h"

#include <set>
#include <string>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

class NodeCategorySet : public BaseObject {
 public:
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
  CHECK_NOT_NULL(env->tracing_agent_writer());
  new NodeCategorySet(env, args.This(), std::move(categories));
}

void NodeCategorySet::Enable(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (!category_set->enabled_ && !categories.empty()) {
    env->tracing_agent_writer()->Enable(categories);
    category_set->enabled_ = true;
  }
}

void NodeCategorySet::Disable(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (category_set->enabled_ && !categories.empty()) {
    env->tracing_agent_writer()->Disable(categories);
    category_set->enabled_ = false;
  }
}

void GetEnabledCategories(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::string categories =
      env->tracing_agent_writer()->agent()->GetEnabledCategories();
  if (!categories.empty()) {
    args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          categories.c_str(),
                          NewStringType::kNormal,
                          categories.size()).ToLocalChecked());
  }
}

// The tracing APIs require category groups to be pointers to long-lived
// strings. Those strings are stored here.
static std::unordered_set<std::string> category_groups;
static Mutex category_groups_mutex;

// Gets a pointer to the category-enabled flags for a tracing category group,
// if tracing is enabled for it.
static const uint8_t* GetCategoryGroupEnabled(const char* category_group) {
  CHECK_NOT_NULL(category_group);
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

static const char* GetCategoryGroup(Environment* env,
                                    const Local<Value> category_value) {
  CHECK(category_value->IsString());

  Utf8Value category(env->isolate(), category_value);
  Mutex::ScopedLock lock(category_groups_mutex);
  // If the value already exists in the set, insertion.first will point
  // to the existing value. Thus, this will maintain a long lived pointer
  // to the category c-string.
  auto insertion = category_groups.insert(category.out());

  // The returned insertion is a pair whose first item is the object that was
  // inserted or that blocked the insertion and second item is a boolean
  // indicating whether it was inserted.
  return insertion.first->c_str();
}

static void Emit(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  // Args: [type, category, name, id, argName, argValue]
  CHECK_GE(args.Length(), 3);

  // Check the category group first, to avoid doing more work if it's not
  // enabled.
  const char* category_group = GetCategoryGroup(env, args[1]);
  const uint8_t* category_group_enabled =
      GetCategoryGroupEnabled(category_group);
  if (*category_group_enabled == 0) return;

  // get trace_event phase
  CHECK(args[0]->IsNumber());
  char phase = static_cast<char>(args[0]->Int32Value(context).ToChecked());

  // get trace_event name
  CHECK(args[2]->IsString());
  Utf8Value name_value(env->isolate(), args[2]);
  const char* name = name_value.out();

  // get trace_event id
  int64_t id = 0;
  bool has_id = false;
  if (args.Length() >= 4 && !args[3]->IsUndefined() && !args[3]->IsNull()) {
    has_id = true;
    CHECK(args[3]->IsNumber());
    id = args[3]->IntegerValue(context).ToChecked();
  }

  // TODO(AndreasMadsen): String values are not supported.
  int32_t num_args = 0;
  const char* arg_names[2];
  uint8_t arg_types[2];
  uint64_t arg_values[2];

  // Create Utf8Value in the function scope to prevent deallocation.
  // The c-string will be copied by TRACE_EVENT_API_ADD_TRACE_EVENT at the end.
  Utf8Value arg1NameValue(env->isolate(), args[4]);
  Utf8Value arg2NameValue(env->isolate(), args[6]);

  if (args.Length() >= 6 &&
      (!args[4]->IsUndefined() || !args[5]->IsUndefined())) {
    num_args = 1;
    arg_types[0] = TRACE_VALUE_TYPE_INT;

    CHECK(args[4]->IsString());
    arg_names[0] = arg1NameValue.out();

    CHECK(args[5]->IsNumber());
    arg_values[0] = args[5]->IntegerValue(context).ToChecked();
  }

  if (args.Length() >= 8 &&
      (!args[6]->IsUndefined() || !args[7]->IsUndefined())) {
    num_args = 2;
    arg_types[1] = TRACE_VALUE_TYPE_INT;

    CHECK(args[6]->IsString());
    arg_names[1] = arg2NameValue.out();

    CHECK(args[7]->IsNumber());
    arg_values[1] = args[7]->IntegerValue(context).ToChecked();
  }

  // The TRACE_EVENT_FLAG_COPY flag indicates that the name and argument
  // name should be copied thus they don't need to long-lived pointers.
  // The category name still won't be copied and thus need to be a long-lived
  // pointer.
  uint32_t flags = TRACE_EVENT_FLAG_COPY;
  if (has_id) {
    flags |= TRACE_EVENT_FLAG_HAS_ID;
  }

  const char* scope = node::tracing::kGlobalScope;
  uint64_t bind_id = node::tracing::kNoId;

  TRACE_EVENT_API_ADD_TRACE_EVENT(
    phase, category_group_enabled, name, scope, id, bind_id,
    num_args, arg_names, arg_types, arg_values,
    flags);
}

static void CategoryGroupEnabled(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const char* category_group = GetCategoryGroup(env, args[0]);
  const uint8_t* category_group_enabled =
      GetCategoryGroupEnabled(category_group);
  args.GetReturnValue().Set(*category_group_enabled > 0);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "emit", Emit);
  env->SetMethod(target, "categoryGroupEnabled", CategoryGroupEnabled);
  env->SetMethod(target, "getEnabledCategories", GetEnabledCategories);

  Local<FunctionTemplate> category_set =
      env->NewFunctionTemplate(NodeCategorySet::New);
  category_set->InstanceTemplate()->SetInternalFieldCount(1);
  env->SetProtoMethod(category_set, "enable", NodeCategorySet::Enable);
  env->SetProtoMethod(category_set, "disable", NodeCategorySet::Disable);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "CategorySet"),
              category_set->GetFunction(env->context()).ToLocalChecked());

  Local<String> isTraceCategoryEnabled =
      FIXED_ONE_BYTE_STRING(env->isolate(), "isTraceCategoryEnabled");
  Local<String> trace = FIXED_ONE_BYTE_STRING(env->isolate(), "trace");

  // Grab the trace and isTraceCategoryEnabled intrinsics from the binding
  // object and expose those to our binding layer.
  Local<Object> binding = context->GetExtrasBindingObject();
  target->Set(context, isTraceCategoryEnabled,
              binding->Get(context, isTraceCategoryEnabled).ToLocalChecked())
                  .FromJust();
  target->Set(context, trace,
              binding->Get(context, trace).ToLocalChecked()).FromJust();
}

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(trace_events, node::Initialize)

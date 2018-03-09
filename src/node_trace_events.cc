#include "node_internals.h"
#include "tracing/agent.h"

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

// The tracing APIs require category groups to be pointers to long-lived
// strings. Those strings are stored here.
static std::unordered_set<std::string> categoryGroups;

// Gets a pointer to the category-enabled flags for a tracing category group,
// if tracing is enabled for it.
static const uint8_t* GetCategoryGroupEnabled(const char* category_group) {
    if (category_group == nullptr) return nullptr;

    return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

static const char* GetCategoryGroup(Environment* env,
                                    const Local<Value> categoryValue) {
  CHECK(categoryValue->IsString());

  Utf8Value category(env->isolate(), categoryValue);
  // If the value already exists in the set, insertion.first will point
  // to the existing value. Thus, this will maintain a long lived pointer
  // to the category c-string.
  auto insertion = categoryGroups.insert(category.out());

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
  Utf8Value nameValue(env->isolate(), args[2]);
  const char* name = nameValue.out();

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

static void EnableTracingCategories(const FunctionCallbackInfo<Value>& args) {
#if NODE_USE_V8_PLATFORM
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsArray());
  Local<Array> categories = args[0].As<Array>();
  size_t length = categories->Length();
  if (length == 0)
    return;
  std::vector<std::string> cats;
  for (size_t n = 0; n < length; n++) {
    Utf8Value val(env->isolate(),
                  categories->Get(env->context(), n).ToLocalChecked());
    cats.push_back(*val);
  }
  EnableTracingCategories(cats);
#elif
  UNREACHABLE();
#endif
}

static void DisableTracingCategories(const FunctionCallbackInfo<Value>& args) {
#if NODE_USE_V8_PLATFORM
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsArray());
  Local<Array> categories = args[0].As<Array>();
  size_t length = categories->Length();
  if (length == 0)
    return;
  std::vector<std::string> cats;
  for (size_t n = 0; n < length; n++) {
    Utf8Value val(env->isolate(),
                  categories->Get(env->context(), n).ToLocalChecked());
    cats.push_back(*val);
  }
  DisableTracingCategories(cats);
#elif
  UNREACHABLE();
#endif
}

static void GetTracingCategories(const FunctionCallbackInfo<Value>& args) {
#if NODE_USE_V8_PLATFORM
  Environment* env = Environment::GetCurrent(args);
  auto categories = GetEnabledTracingCategories();
  Local<Array> ret = Array::New(env->isolate(), categories.size());
  size_t n = 0;
  for (auto it = categories.begin(); it != categories.end(); it++) {
    ret->Set(env->context(), n++,
             String::NewFromUtf8(env->isolate(), (*it).c_str(),
                         v8::NewStringType::kNormal).ToLocalChecked())
                             .FromJust();
  }
  args.GetReturnValue().Set(ret);
#endif
}

void InitializeTraceEvents(Local<Object> target,
                           Local<Value> unused,
                           Local<Context> context,
                           void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "emit", Emit);
  env->SetMethod(target, "categoryGroupEnabled", CategoryGroupEnabled);

  env->SetMethod(target, "enableTracingCategories", EnableTracingCategories);
  env->SetMethod(target, "disableTracingCategories", DisableTracingCategories);
  env->SetMethod(target, "getTracingCategories", GetTracingCategories);
}

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(trace_events, node::InitializeTraceEvents)

#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"

#include "uv.h"
#include "v8.h"
#include "v8-profiler.h"

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::RetainedObjectInfo;
using v8::TryCatch;
using v8::Uint32Array;
using v8::Value;

using AsyncHooks = node::Environment::AsyncHooks;

namespace node {

static const char* const provider_names[] = {
#define V(PROVIDER)                                                           \
  #PROVIDER,
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
};


// Report correct information in a heapdump.

class RetainedAsyncInfo: public RetainedObjectInfo {
 public:
  explicit RetainedAsyncInfo(uint16_t class_id, AsyncWrap* wrap);

  void Dispose() override;
  bool IsEquivalent(RetainedObjectInfo* other) override;
  intptr_t GetHash() override;
  const char* GetLabel() override;
  intptr_t GetSizeInBytes() override;

 private:
  const char* label_;
  const AsyncWrap* wrap_;
  const int length_;
};


RetainedAsyncInfo::RetainedAsyncInfo(uint16_t class_id, AsyncWrap* wrap)
    : label_(provider_names[class_id - NODE_ASYNC_ID_OFFSET]),
      wrap_(wrap),
      length_(wrap->self_size()) {
}


void RetainedAsyncInfo::Dispose() {
  delete this;
}


bool RetainedAsyncInfo::IsEquivalent(RetainedObjectInfo* other) {
  return label_ == other->GetLabel() &&
          wrap_ == static_cast<RetainedAsyncInfo*>(other)->wrap_;
}


intptr_t RetainedAsyncInfo::GetHash() {
  return reinterpret_cast<intptr_t>(wrap_);
}


const char* RetainedAsyncInfo::GetLabel() {
  return label_;
}


intptr_t RetainedAsyncInfo::GetSizeInBytes() {
  return length_;
}


RetainedObjectInfo* WrapperInfo(uint16_t class_id, Local<Value> wrapper) {
  // No class_id should be the provider type of NONE.
  CHECK_GT(class_id, NODE_ASYNC_ID_OFFSET);
  // And make sure the class_id doesn't extend past the last provider.
  CHECK_LE(class_id - NODE_ASYNC_ID_OFFSET, AsyncWrap::PROVIDERS_LENGTH);
  CHECK(wrapper->IsObject());
  CHECK(!wrapper.IsEmpty());

  Local<Object> object = wrapper.As<Object>();
  CHECK_GT(object->InternalFieldCount(), 0);

  AsyncWrap* wrap = Unwrap<AsyncWrap>(object);
  CHECK_NE(nullptr, wrap);

  return new RetainedAsyncInfo(class_id, wrap);
}


// end RetainedAsyncInfo


static void SetupHooks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsObject())
    return env->ThrowTypeError("first argument must be an object");

  // All of init, before, after, destroy are supplied by async_hooks
  // internally, so this should every only be called once. At which time all
  // the functions should be set. Detect this by checking if init !IsEmpty().
  CHECK(env->async_hooks_init_function().IsEmpty());

  Local<Object> fn_obj = args[0].As<Object>();

  Local<Value> init_v = fn_obj->Get(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "init")).ToLocalChecked();
  Local<Value> before_v = fn_obj->Get(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "before")).ToLocalChecked();
  Local<Value> after_v = fn_obj->Get(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "after")).ToLocalChecked();
  Local<Value> destroy_v = fn_obj->Get(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "destroy")).ToLocalChecked();

  CHECK(init_v->IsFunction());
  CHECK(before_v->IsFunction());
  CHECK(after_v->IsFunction());
  CHECK(destroy_v->IsFunction());

  env->set_async_hooks_init_function(init_v.As<Function>());
  env->set_async_hooks_before_function(before_v.As<Function>());
  env->set_async_hooks_after_function(after_v.As<Function>());
  env->set_async_hooks_destroy_function(destroy_v.As<Function>());
}


void AsyncWrap::Initialize(Local<Object> target,
                           Local<Value> unused,
                           Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  env->SetMethod(target, "setupHooks", SetupHooks);
  env->SetMethod(target, "addIdToDestroyList", AddIdToDestroyList);
  env->SetMethod(target, "genIdArray", GenIdArray);
  env->SetMethod(target, "trimIdArray", TrimIdArray);
  env->SetMethod(target, "resetIdArray", ResetIdArray);

  v8::PropertyAttribute ReadOnlyDontDelete =
    static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);

  // The stack of async and trigger ids will be kept in a Float64Array. With a
  // size of 1024 (allowing for 512 recursive calls) it is very unlikely that
  // a new array will ever need to be created, but just in case that occation
  // comes we can handle it. This array will hold allocations that have filled.
  target->ForceSet(context,
                   FIXED_ONE_BYTE_STRING(isolate, "async_id_stack_list"),
                   Array::New(isolate),
                   ReadOnlyDontDelete).FromJust();

  // Attach the uint32_t[] where each slot contains the count of the number of
  // callbacks waiting to be called on a particular event. It can then be
  // incremented/decremented from JS quickly to communicate to C++ if there are
  // any callbacks waiting to be called.
  uint32_t* fields_ptr = env->async_hooks()->fields();
  int fields_count = env->async_hooks()->fields_count();
  Local<ArrayBuffer> fields_ab =
    ArrayBuffer::New(isolate, fields_ptr, fields_count * sizeof(*fields_ptr));
  Local<Uint32Array> fields =
      Uint32Array::New(fields_ab, 0, fields_count);
  target->ForceSet(context,
                   FIXED_ONE_BYTE_STRING(isolate, "async_hook_fields"),
                   fields,
                   ReadOnlyDontDelete).FromJust();

  // The following v8::Float64Array has 5 fields. These fields are shared in
  // this way to allow JS and C++ to read/write each value as quickly as
  // possible. The fields are represented as follows:
  //
  // kAsyncUid: Maintains the state of the next unique id to be assigned.
  //
  // kInitTriggerId: Write the id of the resource resource responsible for a
  //   handle's creation just before calling the new handle's constructor.
  //   After the new handle is constructed kInitTriggerId is set back to 0.
  //
  // kScopedTriggerId: triggerId for all constructors created within the
  //   execution scope of the JS function triggerIdScope(). This value is
  //   superseded by kInitTriggerId, if set.
  double* uid_fields_ptr = env->async_hooks()->uid_fields();
  int uid_fields_count = env->async_hooks()->uid_fields_count();
  Local<ArrayBuffer> uid_fields_ab = ArrayBuffer::New(
      isolate,
      uid_fields_ptr,
      uid_fields_count * sizeof(*uid_fields_ptr));
  Local<Float64Array> uid_fields =
      Float64Array::New(uid_fields_ab, 0, uid_fields_count);
  target->ForceSet(context,
                   FIXED_ONE_BYTE_STRING(isolate, "async_uid_fields"),
                   uid_fields,
                   ReadOnlyDontDelete).FromJust();

  Local<Object> constants = Object::New(isolate);
#define SET_HOOKS_CONSTANT(name)                                              \
  constants->ForceSet(context,                                                \
                      FIXED_ONE_BYTE_STRING(isolate, #name),                  \
                      Integer::New(isolate, AsyncHooks::name),                \
                      ReadOnlyDontDelete).FromJust();
  SET_HOOKS_CONSTANT(kInit);
  SET_HOOKS_CONSTANT(kBefore);
  SET_HOOKS_CONSTANT(kAfter);
  SET_HOOKS_CONSTANT(kDestroy);
  SET_HOOKS_CONSTANT(kActiveHooks);
  SET_HOOKS_CONSTANT(kIdStackIndex);
  SET_HOOKS_CONSTANT(kIdStackSize);
  SET_HOOKS_CONSTANT(kAsyncUidCntr);
  SET_HOOKS_CONSTANT(kInitTriggerId);
  SET_HOOKS_CONSTANT(kScopedTriggerId);
  SET_HOOKS_CONSTANT(kIdStackLimit);
#undef SET_HOOKS_CONSTANT
  target->ForceSet(context,
                   FIXED_ONE_BYTE_STRING(isolate, "constants"),
                   constants,
                   ReadOnlyDontDelete).FromJust();

  Local<Object> async_providers = Object::New(isolate);
#define V(PROVIDER)                                                           \
  async_providers->ForceSet(                                                  \
      context,                                                                \
      FIXED_ONE_BYTE_STRING(isolate, #PROVIDER),                              \
      Integer::New(isolate, AsyncWrap::PROVIDER_ ## PROVIDER),                \
      ReadOnlyDontDelete).FromJust();
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
  target->ForceSet(context,
                   FIXED_ONE_BYTE_STRING(isolate, "Providers"),
                   async_providers,
                   ReadOnlyDontDelete).FromJust();

  // Pass the async_wrap object to Environment::AsyncHooks so the
  // async_id_stack property can be set automatically.
  env->async_hooks()->set_async_wrap_object(target);

  env->set_async_hooks_init_function(Local<Function>());
  env->set_async_hooks_before_function(Local<Function>());
  env->set_async_hooks_after_function(Local<Function>());
  env->set_async_hooks_destroy_function(Local<Function>());
}


void AsyncWrap::GetAsyncId(const FunctionCallbackInfo<Value>& args) {
  AsyncWrap* wrap;
  args.GetReturnValue().Set(-1);
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  args.GetReturnValue().Set(wrap->get_id());
}


void AsyncWrap::DestroyIdsCb(uv_idle_t* handle) {
  uv_idle_stop(handle);

  Environment* env = Environment::from_destroy_ids_idle_handle(handle);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Function> fn = env->async_hooks_destroy_function();

  TryCatch try_catch(env->isolate());

  std::vector<double> destroy_ids_list;
  destroy_ids_list.swap(*env->destroy_ids_list());
  for (auto current_id : destroy_ids_list) {
    // Want each callback to be cleaned up after itself, instead of cleaning
    // them all up after the while() loop completes.
    HandleScope scope(env->isolate());
    Local<Value> argv = Number::New(env->isolate(), current_id);
    MaybeLocal<Value> ret = fn->Call(
        env->context(), Undefined(env->isolate()), 1, &argv);

    if (ret.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
    }
  }

  env->destroy_ids_list()->clear();
}


void AsyncWrap::AsyncReset(const FunctionCallbackInfo<Value>& args) {
  AsyncWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  wrap->Reset();
}


void AsyncWrap::AddIdToDestroyList(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  // This technically shouldn't be needed, since kDestroy should have been
  // checked before calling this function. So make this a CHECK instead?
  if (env->async_hooks()->fields()[AsyncHooks::kDestroy] == 0) {
    return;
  }

  CHECK(args[0]->IsNumber());
  double async_id = args[0]->NumberValue();
  CHECK_GT(async_id, 0);

  if (env->destroy_ids_list()->empty())
    uv_idle_start(env->destroy_ids_idle_handle(), DestroyIdsCb);

  env->destroy_ids_list()->push_back(async_id);
}


void AsyncWrap::GenIdArray(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->async_hooks()->gen_id_array();
}


void AsyncWrap::TrimIdArray(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->async_hooks()->trim_id_array();
}


void AsyncWrap::ResetIdArray(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->async_hooks()->reset_id_array();
}


void LoadAsyncWrapperInfo(Environment* env) {
  HeapProfiler* heap_profiler = env->isolate()->GetHeapProfiler();
#define V(PROVIDER)                                                           \
  heap_profiler->SetWrapperClassInfoProvider(                                 \
      (NODE_ASYNC_ID_OFFSET + AsyncWrap::PROVIDER_ ## PROVIDER), WrapperInfo);
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
}


AsyncWrap::AsyncWrap(Environment* env,
                     Local<Object> object,
                     ProviderType provider)
    : BaseObject(env, object),
      provider_type_(provider) {
  CHECK_NE(provider, PROVIDER_NONE);
  CHECK_GE(object->InternalFieldCount(), 1);

  // Shift provider value over to prevent id collision.
  persistent().SetWrapperClassId(NODE_ASYNC_ID_OFFSET + provider);

  // Use ther Reset() call to call the init() callbacks.
  Reset();
}


AsyncWrap::~AsyncWrap() {
  if (env()->async_hooks()->fields()[AsyncHooks::kDestroy] == 0) {
    return;
  }

  if (env()->destroy_ids_list()->empty())
    uv_idle_start(env()->destroy_ids_idle_handle(), DestroyIdsCb);

  env()->destroy_ids_list()->push_back(get_id());
}


// Generalized call for both the constructor and for handles that are pooled
// and reused over their lifetime. This way a new uid can be assigned when
// the resource is pulled out of the pool and put back into use.
void AsyncWrap::Reset() {
  AsyncHooks* async_hooks = env()->async_hooks();
  id_ = env()->new_async_id();
  trigger_id_ = env()->exchange_init_trigger_id(0);

  // Nothing to execute, so can continue normally.
  if (async_hooks->fields()[AsyncHooks::kInit] == 0) {
    return;
  }

  HandleScope scope(env()->isolate());
  Local<Function> init_fn = env()->async_hooks_init_function();

  Local<Value> argv[] = {
    Number::New(env()->isolate(), get_id()),
    env()->async_hooks()->provider_string(provider_type()),
    object(),
    Number::New(env()->isolate(), get_trigger_id()),
  };

  TryCatch try_catch(env()->isolate());
  MaybeLocal<Value> ret = init_fn->Call(
      env()->context(), object(), arraysize(argv), argv);

  if (ret.IsEmpty()) {
    ClearFatalExceptionHandlers(env());
    FatalException(env()->isolate(), try_catch);
  }
}


Local<Value> AsyncWrap::MakeCallback(const Local<Function> cb,
                                     int argc,
                                     Local<Value>* argv) {
  CHECK(env()->context() == env()->isolate()->GetCurrentContext());

  AsyncHooks* async_hooks = env()->async_hooks();
  Local<Object> context = object();
  Local<Object> domain;
  Local<Value> uid;
  bool has_domain = false;

  Environment::AsyncCallbackScope callback_scope(env());

  if (env()->using_domains()) {
    Local<Value> domain_v = context->Get(env()->domain_string());
    has_domain = domain_v->IsObject();
    if (has_domain) {
      domain = domain_v.As<Object>();
      if (domain->Get(env()->disposed_string())->IsTrue())
        return Local<Value>();
    }
  }

  if (has_domain) {
    Local<Value> enter_v = domain->Get(env()->enter_string());
    if (enter_v->IsFunction()) {
      if (enter_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::AsyncWrap::MakeCallback",
                   "domain enter callback threw, please report this");
      }
    }
  }

  // Want currentId() to return the correct value from the callbacks.
  AsyncHooks::ExecScope exec_scope(env(), get_id(), get_trigger_id());

  if (async_hooks->fields()[AsyncHooks::kBefore] > 0) {
    uid = Number::New(env()->isolate(), get_id());
    Local<Function> fn = env()->async_hooks_before_function();
    TryCatch try_catch(env()->isolate());
    MaybeLocal<Value> ar = fn->Call(
        env()->context(), Undefined(env()->isolate()), 1, &uid);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env());
      FatalException(env()->isolate(), try_catch);
      return Local<Value>();
    }
  }

  // Finally... Get to running the user's callback.
  MaybeLocal<Value> ret = cb->Call(env()->context(), context, argc, argv);

  Local<Value> ret_v;
  if (!ret.ToLocal(&ret_v)) {
    return Local<Value>();
  }

  // If the callback failed then the after() hooks will be called at the end
  // of _fatalException().
  if (async_hooks->fields()[AsyncHooks::kAfter] > 0) {
    if (uid.IsEmpty())
      uid = Number::New(env()->isolate(), get_id());
    Local<Function> fn = env()->async_hooks_after_function();
    TryCatch try_catch(env()->isolate());
    MaybeLocal<Value> ar = fn->Call(
        env()->context(), Undefined(env()->isolate()), 1, &uid);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env());
      FatalException(env()->isolate(), try_catch);
      return Local<Value>();
    }
  }

  // The execution scope of the id and trigger_id only go this far.
  exec_scope.Dispose();

  if (has_domain) {
    Local<Value> exit_v = domain->Get(env()->exit_string());
    if (exit_v->IsFunction()) {
      if (exit_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("node::AsyncWrap::MakeCallback",
                   "domain exit callback threw, please report this");
      }
    }
  }

  if (callback_scope.in_makecallback()) {
    return ret_v;
  }

  Environment::TickInfo* tick_info = env()->tick_info();

  if (tick_info->length() == 0) {
    env()->isolate()->RunMicrotasks();
  }

  // Make sure the stack unwound properly. If there are nested MakeCallback's
  // then it should return early and not reach this code.
  CHECK_EQ(env()->current_async_id(), 0);
  CHECK_EQ(env()->trigger_id(), 0);

  Local<Object> process = env()->process_object();

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret_v;
  }

  MaybeLocal<Value> rcheck =
      env()->tick_callback_function()->Call(env()->context(),
                                            process,
                                            0,
                                            nullptr);

  // Make sure the stack unwound properly.
  CHECK_EQ(env()->current_async_id(), 0);
  CHECK_EQ(env()->trigger_id(), 0);

  return rcheck.IsEmpty() ? Local<Value>() : ret_v;
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(async_wrap, node::AsyncWrap::Initialize)
